#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "synth/app/SynthController.hpp"

#include <string>

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
    }
  };
})();
)JS";

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

@interface SynthAppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler>
@end

@implementation SynthAppDelegate {
    NSWindow* window_;
    WKWebView* webView_;
    std::unique_ptr<synth::app::SynthController> controller_;
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
    [webView_ evaluateJavaScript:source completionHandler:nil];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;

    controller_ = std::make_unique<synth::app::SynthController>();
    if (!controller_->startAudio()) {
        NSLog(@"Failed to start synth audio.");
        [NSApp terminate:nil];
        return;
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

    [window_ makeKeyAndOrderFront:nil];
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
        [self sendResponse:requestId ok:YES payload:controller_->stateJson() error:""];
        return;
    }

    if ([type isEqualToString:@"setParam"]) {
        NSString* path = payload[@"path"];
        id rawValue = payload[@"value"];
        if (path == nil || rawValue == nil) {
            [self sendResponse:requestId ok:NO payload:"null" error:"Missing path or value."];
            return;
        }

        std::string errorMessage;
        bool success = false;

        if ([rawValue isKindOfClass:[NSNumber class]]) {
            success = controller_->setParam(
                std::string([path UTF8String]),
                [(NSNumber*)rawValue doubleValue],
                &errorMessage);
        } else if ([rawValue isKindOfClass:[NSString class]]) {
            success = controller_->setParam(
                std::string([path UTF8String]),
                std::string([(NSString*)rawValue UTF8String]),
                &errorMessage);
        } else {
            errorMessage = "Unsupported JS value type.";
        }

        if (!success) {
            [self sendResponse:requestId ok:NO payload:"null" error:errorMessage];
            return;
        }

        [self sendResponse:requestId ok:YES payload:controller_->stateJson() error:""];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
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
        NSApplication* application = [NSApplication sharedApplication];
        SynthAppDelegate* delegate = [[SynthAppDelegate alloc] init];
        [application setDelegate:delegate];
        [application run];
    }

    return 0;
}
