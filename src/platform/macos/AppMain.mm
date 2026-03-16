#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "synth/app/SynthController.hpp"
#include "synth/core/CrashDiagnostics.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

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
    }
  };
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

}  // namespace

static void handleUncaughtNsException(NSException* exception) {
    const std::string name = exception.name != nil ? std::string([[exception.name description] UTF8String]) : "Unknown";
    const std::string reason =
        exception.reason != nil ? std::string([[exception.reason description] UTF8String]) : "Unknown";
    synth::core::CrashDiagnostics::noteObjectiveCException(name, reason);
}

@interface SynthAppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler>
@end

@implementation SynthAppDelegate {
    NSWindow* window_;
    WKWebView* webView_;
    std::unique_ptr<synth::app::SynthController> controller_;
    BOOL debugBridge_;
    BOOL debugCrash_;
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
    if (webView_ == nil) {
        return;
    }

    std::string script = "window.__synthNativeReceive(" + std::to_string(static_cast<long long>(requestId)) + ", "
        + (ok ? "true" : "false") + ", ";
    script += ok ? payloadJson : "null";
    script += ", ";
    script += ok ? "null" : ("\"" + escapeJavaScriptString(errorMessage) + "\"");
    script += ");";

    NSString* source = [NSString stringWithUTF8String:script.c_str()];
    [webView_ evaluateJavaScript:source
               completionHandler:^(id result, NSError* error) {
                   (void)result;
                   if (error != nil && controller_ != nullptr) {
                       controller_->logger().error(
                           "Bridge evaluateJavaScript failed: " + std::string([[error localizedDescription] UTF8String]));
                   }
               }];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    debugBridge_ = envFlagEnabled("SYNTH_DEBUG_BRIDGE") || envFlagEnabled("SYNTH_DEBUG_ROBIN");
    debugCrash_ = envFlagEnabled("SYNTH_DEBUG_CRASH");

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    controller_ = std::make_unique<synth::app::SynthController>();
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

    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    [config setUserContentController:userContentController];

    webView_ = [[WKWebView alloc] initWithFrame:[[window_ contentView] bounds] configuration:config];
    [webView_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
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

    if ([type isEqualToString:@"getState"]) {
        if (debugCrash_) {
            controller_->crashDiagnostics().breadcrumb("Bridge getState request.");
        }
        [self sendResponse:requestId ok:YES payload:controller_->stateJson() error:""];
        return;
    }

    if ([type isEqualToString:@"setParam"] || [type isEqualToString:@"setParamFast"]) {
        NSString* path = payload[@"path"];
        id rawValue = payload[@"value"];
        if (path == nil || rawValue == nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Missing path or value."];
            return;
        }

        const bool wantsState = [type isEqualToString:@"setParam"];
        std::string errorMessage;
        bool success = false;
        const auto paramStart = std::chrono::steady_clock::now();
        const std::string pathString([path UTF8String]);

        if (debugCrash_) {
            controller_->crashDiagnostics().breadcrumb(
                "Bridge " + std::string([type UTF8String]) + " start path=" + pathString);
        }

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
                    "Bridge " + std::string([type UTF8String]) + " failed path=" + pathString + " error=" + errorMessage);
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
                    "Bridge " + std::string([type UTF8String])
                    + " " + std::string([path UTF8String])
                    + " paramMs=" + std::to_string(paramMs)
                    + " payloadMs=" + std::to_string(payloadMs));
            }
        }

        if (debugCrash_) {
            controller_->crashDiagnostics().breadcrumb(
                "Bridge " + std::string([type UTF8String]) + " ok path=" + pathString);
        }

        [self sendResponse:requestId ok:YES payload:payloadJson error:""];
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

@end

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        NSSetUncaughtExceptionHandler(&handleUncaughtNsException);
        NSApplication* application = [NSApplication sharedApplication];
        SynthAppDelegate* delegate = [[SynthAppDelegate alloc] init];
        [application setDelegate:delegate];
        [application run];
    }

    return 0;
}
