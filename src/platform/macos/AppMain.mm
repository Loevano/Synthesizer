#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "synth/app/SynthHost.hpp"
#include "synth/core/CrashDiagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kBridgeScript = R"JS(
(() => {
  const pending = new Map();
  let nextRequestId = 1;

  window.__synthNativeReceive = (requestId, ok, payload, error) => {
    const entry = pending.get(requestId);
    if (!entry) {
      return;
    }

    pending.delete(requestId);
    if (ok) {
      entry.resolve(payload);
    } else {
      entry.reject(new Error(error || "Native bridge error"));
    }
  };

  const request = (type, payload = {}) => {
    const requestId = nextRequestId++;
    return new Promise((resolve, reject) => {
      pending.set(requestId, { resolve, reject });
      window.webkit.messageHandlers.synth.postMessage({ requestId, type, payload });
    });
  };

  window.synth = {
    getState() {
      return request("getState");
    },
    setParam(path, value) {
      return request("setParam", { path, value });
    },
    setParamFast(path, value) {
      return request("setParamFast", { path, value });
    },
    listPatches() {
      return request("listPatches");
    },
    loadPatch(fileName) {
      return request("loadPatch", { fileName });
    },
    savePatch(payload) {
      return request("savePatch", payload);
    }
  };

  const sendClientError = (kind, payload) => {
    try {
      window.webkit.messageHandlers.synth.postMessage({
        requestId: 0,
        type: kind,
        payload,
      });
    } catch (error) {
      // Keep the page alive even if the native bridge is unavailable.
    }
  };

  window.addEventListener("error", (event) => {
    sendClientError("jsError", {
      message: event.message || "Unknown JS error",
      source: event.filename || "",
      line: event.lineno || 0,
      column: event.colno || 0,
    });
  });

  window.addEventListener("unhandledrejection", (event) => {
    const reason = event.reason instanceof Error
      ? (event.reason.stack || event.reason.message)
      : String(event.reason || "Unknown rejection");
    sendClientError("jsRejection", { message: reason });
  });
})();
)JS";

bool envFlagEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return false;
    }

    const std::string_view flag{value};
    return flag != "0" && flag != "false" && flag != "FALSE" && flag != "off" && flag != "OFF";
}

std::string escapeJavaScriptString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::string jsonStringFromFoundationObject(id object) {
    if (object == nil || ![NSJSONSerialization isValidJSONObject:object]) {
        return "null";
    }

    NSError* error = nil;
    NSData* jsonData = [NSJSONSerialization dataWithJSONObject:object options:0 error:&error];
    if (jsonData == nil || error != nil) {
        return "null";
    }

    NSString* jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    if (jsonString == nil) {
        return "null";
    }

    return std::string([jsonString UTF8String]);
}

std::string trimAscii(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string sanitizePatchName(std::string_view rawName) {
    std::string sanitized;
    sanitized.reserve(rawName.size());

    bool previousWasSeparator = false;
    for (const char ch : rawName) {
        const unsigned char code = static_cast<unsigned char>(ch);
        if (std::isalnum(code)) {
            sanitized.push_back(ch);
            previousWasSeparator = false;
            continue;
        }

        if (ch == ' ' || ch == '-' || ch == '_') {
            if (!previousWasSeparator && !sanitized.empty()) {
                sanitized.push_back(' ');
                previousWasSeparator = true;
            }
        }
    }

    sanitized = trimAscii(std::move(sanitized));
    if (sanitized.empty()) {
        sanitized = "Patch";
    }
    if (sanitized.size() > 80) {
        sanitized.resize(80);
        sanitized = trimAscii(std::move(sanitized));
    }
    return sanitized;
}

std::string displayNameFromFileStem(std::string stem) {
    std::replace(stem.begin(), stem.end(), '-', ' ');
    std::replace(stem.begin(), stem.end(), '_', ' ');
    stem = trimAscii(std::move(stem));
    return stem.empty() ? "Patch" : stem;
}

std::filesystem::path developmentPatchDirectory() {
#if defined(SYNTH_PROJECT_SOURCE_DIR)
    return std::filesystem::path(SYNTH_PROJECT_SOURCE_DIR) / "Patches";
#else
    return {};
#endif
}

std::filesystem::path applicationSupportPatchDirectory() {
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSURL* applicationSupportUrl =
        [[fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] firstObject];
    if (applicationSupportUrl == nil) {
        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
            return std::filesystem::path(home) / "Library" / "Application Support" / "Synthesizer" / "Patches";
        }
        return std::filesystem::path("Patches");
    }

    NSURL* synthUrl = [applicationSupportUrl URLByAppendingPathComponent:@"Synthesizer" isDirectory:YES];
    NSURL* patchesUrl = [synthUrl URLByAppendingPathComponent:@"Patches" isDirectory:YES];
    return std::filesystem::path([[patchesUrl path] UTF8String]);
}

bool pathHasPrefix(const std::filesystem::path& path, const std::filesystem::path& prefix) {
    const auto normalizedPath = path.lexically_normal();
    const auto normalizedPrefix = prefix.lexically_normal();
    auto mismatch = std::mismatch(
        normalizedPrefix.begin(),
        normalizedPrefix.end(),
        normalizedPath.begin(),
        normalizedPath.end());
    return mismatch.first == normalizedPrefix.end();
}

void applyLaunchArgumentsToEnvironment(int argc, const char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index] != nullptr ? std::string_view(argv[index]) : std::string_view{};
        if (argument == "--debug-crash") {
            setenv("SYNTH_DEBUG_CRASH", "1", 1);
            setenv("SYNTH_DEBUG_BRIDGE", "1", 1);
            setenv("SYNTH_DEBUG_ROBIN", "1", 1);
            continue;
        }
        if (argument == "--debug-bridge") {
            setenv("SYNTH_DEBUG_BRIDGE", "1", 1);
            continue;
        }
        if (argument == "--debug-robin") {
            setenv("SYNTH_DEBUG_ROBIN", "1", 1);
        }
    }
}

std::string buildBridgeResponseScript(NSInteger requestId,
                                      BOOL ok,
                                      const std::string& payloadJson,
                                      const std::string& errorMessage) {
    std::string script;
    script.reserve(96 + payloadJson.size() + errorMessage.size());

    script += "window.__synthNativeReceive(";
    script += std::to_string(static_cast<long long>(requestId));
    script += ", ";
    script += ok ? "true" : "false";
    script += ", ";
    script += ok ? payloadJson : "null";
    script += ", ";
    if (ok) {
        script += "null";
    } else {
        script += "\"";
        script += escapeJavaScriptString(errorMessage);
        script += "\"";
    }
    script += ");";

    return script;
}

}  // namespace

static void handleUncaughtNsException(NSException* exception) {
    const std::string name = exception.name != nil ? std::string([[exception.name description] UTF8String]) : "Unknown";
    const std::string reason =
        exception.reason != nil ? std::string([[exception.reason description] UTF8String]) : "Unknown";
    synth::core::CrashDiagnostics::noteObjectiveCException(name, reason);
}

@interface SynthAppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler, WKUIDelegate>
@end

@implementation SynthAppDelegate {
    NSWindow* window_;
    WKWebView* webView_;
    std::unique_ptr<synth::app::SynthHost> controller_;
    dispatch_queue_t bridgeQueue_;
    BOOL debugBridge_;
    BOOL debugCrash_;
    BOOL debugUi_;
}

- (std::filesystem::path)resolvedPatchDirectoryUsingProjectDirectory:(BOOL*)usingProjectDirectory {
    const std::filesystem::path devDirectory = developmentPatchDirectory();
    NSString* bundlePathString = [[NSBundle mainBundle] bundlePath];
    const std::filesystem::path bundlePath =
        bundlePathString != nil ? std::filesystem::path([bundlePathString UTF8String]) : std::filesystem::path{};
    const std::filesystem::path projectDirectory = devDirectory.parent_path();

    if (!devDirectory.empty()
        && !bundlePath.empty()
        && std::filesystem::exists(projectDirectory)
        && pathHasPrefix(bundlePath, projectDirectory)) {
        if (usingProjectDirectory != nullptr) {
            *usingProjectDirectory = YES;
        }
        return devDirectory;
    }

    if (usingProjectDirectory != nullptr) {
        *usingProjectDirectory = NO;
    }
    return applicationSupportPatchDirectory();
}

- (BOOL)ensurePatchDirectory:(std::filesystem::path*)directoryPath error:(std::string*)errorMessage {
    BOOL usingProjectDirectory = NO;
    std::filesystem::path resolvedDirectory = [self resolvedPatchDirectoryUsingProjectDirectory:&usingProjectDirectory];

    try {
        std::filesystem::create_directories(resolvedDirectory);
    } catch (const std::exception& exception) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create patch directory: " + std::string(exception.what());
        }
        return NO;
    }

    if (directoryPath != nullptr) {
        *directoryPath = std::move(resolvedDirectory);
    }
    return YES;
}

- (std::optional<std::filesystem::path>)patchPathForFileName:(NSString*)fileName error:(std::string*)errorMessage {
    if (fileName == nil) {
        if (errorMessage != nullptr) {
            *errorMessage = "Missing patch file name.";
        }
        return std::nullopt;
    }

    const std::string candidate([fileName UTF8String]);
    if (candidate.empty() || candidate.find('/') != std::string::npos || candidate.find('\\') != std::string::npos
        || candidate == "." || candidate == "..") {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid patch file name.";
        }
        return std::nullopt;
    }

    std::filesystem::path directoryPath;
    if (![self ensurePatchDirectory:&directoryPath error:errorMessage]) {
        return std::nullopt;
    }

    std::filesystem::path patchPath(candidate);
    if (patchPath.has_parent_path()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Patch file name must not include directories.";
        }
        return std::nullopt;
    }

    if (patchPath.extension() != ".json") {
        patchPath += ".json";
    }

    return directoryPath / patchPath.filename();
}

- (NSDictionary*)patchMetadataForPath:(const std::filesystem::path&)patchPath {
    NSString* fileName = [NSString stringWithUTF8String:patchPath.filename().string().c_str()];
    NSString* displayName = [NSString stringWithUTF8String:displayNameFromFileStem(patchPath.stem().string()).c_str()];
    NSMutableDictionary* metadata = [@{
        @"fileName": fileName,
        @"name": displayName,
        @"isDefault": @([fileName isEqualToString:@"default-patch.json"]),
    } mutableCopy];

    NSData* patchData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:patchPath.string().c_str()]];
    if (patchData != nil) {
        NSError* error = nil;
        id patchJson = [NSJSONSerialization JSONObjectWithData:patchData options:0 error:&error];
        NSDictionary* patchDictionary = [patchJson isKindOfClass:[NSDictionary class]] ? (NSDictionary*)patchJson : nil;
        NSString* patchName = [patchDictionary[@"name"] isKindOfClass:[NSString class]] ? patchDictionary[@"name"] : nil;
        if (patchName != nil && patchName.length > 0) {
            metadata[@"name"] = patchName;
        }
    }

    return metadata;
}

- (std::string)patchLibraryJsonWithError:(std::string*)errorMessage {
    std::filesystem::path directoryPath;
    if (![self ensurePatchDirectory:&directoryPath error:errorMessage]) {
        return "null";
    }

    BOOL usingProjectDirectory = NO;
    (void)[self resolvedPatchDirectoryUsingProjectDirectory:&usingProjectDirectory];

    NSMutableArray* patches = [NSMutableArray array];
    try {
        std::vector<std::filesystem::path> patchPaths;
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }
            patchPaths.push_back(entry.path());
        }

        std::sort(patchPaths.begin(), patchPaths.end(), [](const auto& left, const auto& right) {
            const bool leftIsDefault = left.filename() == "default-patch.json";
            const bool rightIsDefault = right.filename() == "default-patch.json";
            if (leftIsDefault != rightIsDefault) {
                return leftIsDefault;
            }
            return left.filename().string() < right.filename().string();
        });

        for (const auto& patchPath : patchPaths) {
            [patches addObject:[self patchMetadataForPath:patchPath]];
        }
    } catch (const std::exception& exception) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not list patches: " + std::string(exception.what());
        }
        return "null";
    }

    NSDictionary* payload = @{
        @"directory": [NSString stringWithUTF8String:directoryPath.string().c_str()],
        @"usingProjectDirectory": @(usingProjectDirectory),
        @"defaultPatchFileName": @"default-patch.json",
        @"patches": patches,
    };
    return jsonStringFromFoundationObject(payload);
}

- (void)bringMainWindowToFront {
    if (window_ == nil) {
        return;
    }

    [window_ setIsVisible:YES];
    [window_ deminiaturize:nil];
    [window_ makeKeyAndOrderFront:nil];
    [window_ orderFrontRegardless];
    [window_ orderWindow:NSWindowAbove relativeTo:0];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)sendResponse:(NSInteger)requestId
                  ok:(BOOL)ok
              payload:(const std::string&)payloadJson
               error:(const std::string&)errorMessage {
    const std::string script = buildBridgeResponseScript(requestId, ok, payloadJson, errorMessage);
    NSString* source = [[NSString alloc] initWithBytes:script.data()
                                                length:script.size()
                                              encoding:NSUTF8StringEncoding];
    if (source == nil) {
        if (controller_ != nullptr) {
            controller_->logger().error("Bridge response encoding failed.");
        }
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        if (webView_ == nil) {
            return;
        }

        [webView_ evaluateJavaScript:source
                   completionHandler:^(id result, NSError* error) {
                       (void)result;
                       if (error != nil && controller_ != nullptr) {
                           controller_->logger().error(
                               "Bridge evaluateJavaScript failed: " + std::string([[error localizedDescription] UTF8String]));
                       }
                   }];
    });
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    debugBridge_ = envFlagEnabled("SYNTH_DEBUG_BRIDGE") || envFlagEnabled("SYNTH_DEBUG_ROBIN");
    debugCrash_ = envFlagEnabled("SYNTH_DEBUG_CRASH");
    debugUi_ = envFlagEnabled("SYNTH_DEBUG_UI") || envFlagEnabled("SYNTH_DEBUG_ROBIN");
    bridgeQueue_ = dispatch_queue_create("com.loevano.synth.bridge", DISPATCH_QUEUE_SERIAL);

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    controller_ = std::make_unique<synth::app::SynthHost>();
    if (!controller_->startAudio()) {
        NSLog(@"Failed to start synth audio.");
        [NSApp terminate:nil];
        return;
    }

    if (debugBridge_ && controller_ != nullptr) {
        controller_->logger().debug("SYNTH_DEBUG_BRIDGE enabled.");
    }

    const NSRect frame = NSMakeRect(0.0, 0.0, 1240.0, 860.0);
    const NSUInteger styleMask =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
        | NSWindowStyleMaskResizable;

    window_ = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [window_ setTitle:@"Synthesizer"];
    [window_ center];
    [window_ setReleasedWhenClosed:NO];
    [window_ setRestorable:NO];
    [window_ setCollectionBehavior:NSWindowCollectionBehaviorMoveToActiveSpace];

    WKUserContentController* userContentController = [[WKUserContentController alloc] init];
    [userContentController addScriptMessageHandler:self name:@"synth"];
    NSString* bootstrapSource = [NSString stringWithUTF8String:kBridgeScript];
    WKUserScript* bootstrapScript =
        [[WKUserScript alloc] initWithSource:bootstrapSource
                               injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                            forMainFrameOnly:YES];
    [userContentController addUserScript:bootstrapScript];
    NSString* flagsSource = [NSString
        stringWithFormat:@"window.__SYNTH_FLAGS__ = Object.freeze({ debugUi: %@ });",
                         debugUi_ ? @"true" : @"false"];
    WKUserScript* flagsScript =
        [[WKUserScript alloc] initWithSource:flagsSource
                               injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                            forMainFrameOnly:YES];
    [userContentController addUserScript:flagsScript];

    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    [config setUserContentController:userContentController];

    webView_ = [[WKWebView alloc] initWithFrame:[[window_ contentView] bounds] configuration:config];
    [webView_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [webView_ setUIDelegate:self];
    [[window_ contentView] addSubview:webView_];

    NSURL* indexUrl = [[NSBundle mainBundle] URLForResource:@"index" withExtension:@"html" subdirectory:@"web"];
    if (indexUrl != nil) {
        NSURL* readAccessUrl = [indexUrl URLByDeletingLastPathComponent];
        [webView_ loadFileURL:indexUrl allowingReadAccessToURL:readAccessUrl];
    } else {
        NSString* fallbackHtml =
            @"<html><body style='background:#111;color:#f3f0dc;font-family:-apple-system;padding:32px;'>"
             "<h1>Synthesizer</h1><p>Could not load bundled web UI assets.</p></body></html>";
        [webView_ loadHTMLString:fallbackHtml baseURL:nil];
    }

    [self bringMainWindowToFront];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self bringMainWindowToFront];
    });

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self bringMainWindowToFront];
    });
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
    (void)notification;
    [self bringMainWindowToFront];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)userContentController;

    if (![message.name isEqualToString:@"synth"]) {
        return;
    }

    NSDictionary* body = [message.body isKindOfClass:[NSDictionary class]] ? (NSDictionary*)message.body : nil;
    NSNumber* requestIdNumber = body[@"requestId"];
    NSString* type = body[@"type"];
    NSDictionary* payload = [body[@"payload"] isKindOfClass:[NSDictionary class]] ? body[@"payload"] : @{};

    if (requestIdNumber == nil || type == nil || controller_ == nullptr) {
        return;
    }

    const NSInteger requestId = [requestIdNumber integerValue];

    if ([type isEqualToString:@"jsError"] || [type isEqualToString:@"jsRejection"]) {
        NSString* messageText = payload[@"message"];
        NSString* source = payload[@"source"];
        NSNumber* line = payload[@"line"];
        NSNumber* column = payload[@"column"];

        std::string logLine = "Web UI ";
        logLine += [type isEqualToString:@"jsError"] ? "error: " : "rejection: ";
        logLine += messageText != nil ? std::string([messageText UTF8String]) : "Unknown";

        if (source != nil && [source length] > 0) {
            logLine += " source=" + std::string([source UTF8String]);
        }
        if (line != nil) {
            logLine += " line=" + std::to_string([line longLongValue]);
        }
        if (column != nil) {
            logLine += " column=" + std::to_string([column longLongValue]);
        }

        controller_->logger().error(logLine);
        if (debugCrash_) {
            controller_->crashDiagnostics().breadcrumb(logLine);
        }
        return;
    }

    if ([type isEqualToString:@"getState"]) {
        dispatch_async(bridgeQueue_, ^{
            @autoreleasepool {
                if (debugCrash_) {
                    controller_->crashDiagnostics().breadcrumb("Bridge getState request.");
                }
                [self sendResponse:requestId ok:YES payload:controller_->stateJson() error:""];
            }
        });
        return;
    }

    if ([type isEqualToString:@"listPatches"]) {
        std::string errorMessage;
        const std::string payloadJson = [self patchLibraryJsonWithError:&errorMessage];
        if (payloadJson == "null" && !errorMessage.empty()) {
            [self sendResponse:requestId ok:NO payload:"null" error:errorMessage];
            return;
        }

        [self sendResponse:requestId ok:YES payload:payloadJson error:""];
        return;
    }

    if ([type isEqualToString:@"loadPatch"]) {
        NSString* fileName = [payload[@"fileName"] isKindOfClass:[NSString class]] ? payload[@"fileName"] : nil;
        std::string errorMessage;
        const auto patchPath = [self patchPathForFileName:fileName error:&errorMessage];
        if (!patchPath.has_value()) {
            [self sendResponse:requestId ok:NO payload:"null" error:errorMessage];
            return;
        }

        NSData* patchData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:patchPath->string().c_str()]];
        if (patchData == nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Could not read patch file."];
            return;
        }

        NSError* parseError = nil;
        id patchJson = [NSJSONSerialization JSONObjectWithData:patchData options:0 error:&parseError];
        if (patchJson == nil || ![patchJson isKindOfClass:[NSDictionary class]] || parseError != nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Patch file is not valid JSON."];
            return;
        }

        NSString* patchJsonString = [[NSString alloc] initWithData:patchData encoding:NSUTF8StringEncoding];
        if (patchJsonString == nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Patch file is not UTF-8 JSON."];
            return;
        }

        [self sendResponse:requestId ok:YES payload:std::string([patchJsonString UTF8String]) error:""];
        return;
    }

    if ([type isEqualToString:@"savePatch"]) {
        NSDictionary* patchPayload = [payload[@"patch"] isKindOfClass:[NSDictionary class]] ? payload[@"patch"] : nil;
        NSString* fileName = [payload[@"fileName"] isKindOfClass:[NSString class]] ? payload[@"fileName"] : nil;
        NSString* patchName = [payload[@"name"] isKindOfClass:[NSString class]] ? payload[@"name"] : nil;

        if (patchPayload == nil || ![NSJSONSerialization isValidJSONObject:patchPayload]) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Missing or invalid patch payload."];
            return;
        }

        if (fileName == nil) {
            const std::string sanitizedName = sanitizePatchName(
                patchName != nil ? std::string([patchName UTF8String]) : std::string{});
            fileName = [NSString stringWithUTF8String:(sanitizedName + ".json").c_str()];
        }

        std::string errorMessage;
        const auto patchPath = [self patchPathForFileName:fileName error:&errorMessage];
        if (!patchPath.has_value()) {
            [self sendResponse:requestId ok:NO payload:"null" error:errorMessage];
            return;
        }

        NSError* jsonError = nil;
        NSData* patchData = [NSJSONSerialization dataWithJSONObject:patchPayload options:NSJSONWritingPrettyPrinted error:&jsonError];
        if (patchData == nil || jsonError != nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Could not serialize patch JSON."];
            return;
        }

        if (![patchData writeToFile:[NSString stringWithUTF8String:patchPath->string().c_str()] atomically:YES]) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Could not write patch file."];
            return;
        }

        NSDictionary* metadata = [self patchMetadataForPath:*patchPath];
        [self sendResponse:requestId ok:YES payload:jsonStringFromFoundationObject(metadata) error:""];
        return;
    }

    if ([type isEqualToString:@"setParam"] || [type isEqualToString:@"setParamFast"]) {
        NSString* path = payload[@"path"];
        id rawValue = payload[@"value"];
        if (path == nil || rawValue == nil) {
            dispatch_async(bridgeQueue_, ^{
                @autoreleasepool {
                    [self sendResponse:requestId ok:NO payload:"null" error:"Missing path or value."];
                }
            });
            return;
        }

        const std::string pathString([path UTF8String]);
        const bool wantsState = [type isEqualToString:@"setParam"];
        const std::string typeString([type UTF8String]);

        if (debugCrash_) {
            controller_->crashDiagnostics().breadcrumb(
                "Bridge " + typeString + " start path=" + pathString);
        }

        dispatch_async(bridgeQueue_, ^{
            @autoreleasepool {
                std::string errorMessage;
                bool success = false;
                const auto paramStart = std::chrono::steady_clock::now();

                if ([rawValue isKindOfClass:[NSNumber class]]) {
                    success = controller_->setParam(
                        pathString,
                        [(NSNumber*)rawValue doubleValue],
                        &errorMessage);
                } else if ([rawValue isKindOfClass:[NSString class]]) {
                    success = controller_->setParam(
                        pathString,
                        std::string([(NSString*)rawValue UTF8String]),
                        &errorMessage);
                } else {
                    errorMessage = "Unsupported JS value type.";
                }

                if (!success) {
                    if (debugCrash_) {
                        controller_->crashDiagnostics().breadcrumb(
                            "Bridge " + typeString + " failed path=" + pathString + " error=" + errorMessage);
                    }
                    [self sendResponse:requestId ok:NO payload:"null" error:errorMessage];
                    return;
                }

                const auto afterParam = std::chrono::steady_clock::now();
                std::string payloadJson = "true";
                if (wantsState) {
                    payloadJson = controller_->stateJson();
                }
                const auto afterPayload = std::chrono::steady_clock::now();

                if (debugBridge_) {
                    const auto paramMs = std::chrono::duration<double, std::milli>(afterParam - paramStart).count();
                    const auto payloadMs = std::chrono::duration<double, std::milli>(afterPayload - afterParam).count();
                    if (controller_ != nullptr) {
                        controller_->logger().debug(
                            "Bridge " + typeString
                            + " " + pathString
                            + " paramMs=" + std::to_string(paramMs)
                            + " payloadMs=" + std::to_string(payloadMs));
                    }
                }

                if (debugCrash_) {
                    controller_->crashDiagnostics().breadcrumb(
                        "Bridge " + typeString + " ok path=" + pathString);
                }

                [self sendResponse:requestId ok:YES payload:payloadJson error:""];
            }
        });
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (BOOL)applicationShouldSaveApplicationState:(NSApplication*)sender {
    (void)sender;
    return NO;
}

- (BOOL)applicationShouldRestoreApplicationState:(NSApplication*)sender {
    (void)sender;
    return NO;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)flag {
    (void)sender;
    if (!flag && window_ != nil) {
        [self bringMainWindowToFront];
    }
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    (void)notification;
    if (controller_ != nullptr) {
        controller_->stopAudio();
    }
}

- (void)webView:(WKWebView*)webView
runJavaScriptAlertPanelWithMessage:(NSString*)message
initiatedByFrame:(WKFrameInfo*)frame
completionHandler:(void (^)(void))completionHandler {
    (void)webView;
    (void)frame;

    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Synthesizer"];
    [alert setInformativeText:message ?: @""];
    [alert addButtonWithTitle:@"OK"];

    if (window_ != nil) {
        [alert beginSheetModalForWindow:window_
                      completionHandler:^(__unused NSModalResponse returnCode) {
                          completionHandler();
                      }];
        return;
    }

    [alert runModal];
    completionHandler();
}

- (void)webView:(WKWebView*)webView
runJavaScriptConfirmPanelWithMessage:(NSString*)message
initiatedByFrame:(WKFrameInfo*)frame
completionHandler:(void (^)(BOOL result))completionHandler {
    (void)webView;
    (void)frame;

    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Synthesizer"];
    [alert setInformativeText:message ?: @""];
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    if (window_ != nil) {
        [alert beginSheetModalForWindow:window_
                      completionHandler:^(NSModalResponse returnCode) {
                          completionHandler(returnCode == NSAlertFirstButtonReturn);
                      }];
        return;
    }

    completionHandler([alert runModal] == NSAlertFirstButtonReturn);
}

- (void)webView:(WKWebView*)webView
runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
defaultText:(NSString*)defaultText
initiatedByFrame:(WKFrameInfo*)frame
completionHandler:(void (^)(NSString* _Nullable result))completionHandler {
    (void)webView;
    (void)frame;

    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Synthesizer"];
    [alert setInformativeText:prompt ?: @""];
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* inputField = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, 0.0, 320.0, 24.0)];
    [inputField setStringValue:defaultText ?: @""];
    [alert setAccessoryView:inputField];

    if (window_ != nil) {
        [alert beginSheetModalForWindow:window_
                      completionHandler:^(NSModalResponse returnCode) {
                          if (returnCode == NSAlertFirstButtonReturn) {
                              completionHandler([inputField stringValue]);
                              return;
                          }
                          completionHandler(nil);
                      }];
        return;
    }

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        completionHandler([inputField stringValue]);
        return;
    }

    completionHandler(nil);
}

@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        NSSetUncaughtExceptionHandler(&handleUncaughtNsException);
        applyLaunchArgumentsToEnvironment(argc, argv);
        NSApplication* application = [NSApplication sharedApplication];
        SynthAppDelegate* delegate = [[SynthAppDelegate alloc] init];
        [application setDelegate:delegate];
        [application run];
    }

    return 0;
}
