#import "PresenterBridge.h"
#include "presenter_api.h"
#include <os/lock.h>

// ---------------------------------------------------------------------------
// PresenterSlideInfo  —  immutable snapshot
// ---------------------------------------------------------------------------

@implementation PresenterSlideInfo {
    NSInteger _slideIndex;
    NSInteger _slideCount;
    NSString* _slideLabel;
    NSString* _notes;
    NSString* _presentationTitle;
    BOOL      _canGoNext;
    BOOL      _canGoPrev;
}

- (instancetype)initWithHandle:(PresenterHandle)h {
    self = [super init];
    if (self) {
        PresenterPresentationInfo presentationInfo = presenter_presentation_info(h);
        PresenterSlideData slideInfo = presenter_current_slide_info(h);
        _slideIndex        = presentationInfo.current_slide_index;
        _slideCount        = presentationInfo.slide_count;
        _slideLabel        = @(presentationInfo.current_slide_label);
        _notes             = @(slideInfo.notes);
        _presentationTitle = @(presentationInfo.title);
        _canGoNext         = (BOOL)presentationInfo.can_go_next;
        _canGoPrev         = (BOOL)presentationInfo.can_go_prev;
    }
    return self;
}

- (NSInteger)slideIndex        { return _slideIndex; }
- (NSInteger)slideCount        { return _slideCount; }
- (NSString*)slideLabel        { return _slideLabel; }
- (NSString*)notes             { return _notes; }
- (NSString*)presentationTitle { return _presentationTitle; }
- (BOOL)canGoNext              { return _canGoNext; }
- (BOOL)canGoPrev              { return _canGoPrev; }

@end

// ---------------------------------------------------------------------------
// PresenterBridge
// ---------------------------------------------------------------------------

@implementation PresenterBridge {
    PresenterHandle _handle;
    os_unfair_lock  _lock;
}

- (nullable instancetype)initWithSlidesPath:(NSString*)slidesPath {
    self = [super init];
    if (!self) return nil;

    _lock = OS_UNFAIR_LOCK_INIT;

    NSString* resourcePath = NSBundle.mainBundle.resourcePath;
    _handle = presenter_load(slidesPath.UTF8String, resourcePath.UTF8String);
    if (!_handle) return nil;

    return self;
}

- (void)dealloc {
    if (_handle) {
        presenter_free(_handle);
        _handle = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Navigation

- (void)next {
    os_unfair_lock_lock(&_lock);
    presenter_next(_handle);
    os_unfair_lock_unlock(&_lock);
}

- (void)prev {
    os_unfair_lock_lock(&_lock);
    presenter_prev(_handle);
    os_unfair_lock_unlock(&_lock);
}

- (void)goTo:(NSInteger)index {
    os_unfair_lock_lock(&_lock);
    presenter_go_to(_handle, (int)index);
    os_unfair_lock_unlock(&_lock);
}

// ---------------------------------------------------------------------------
// State snapshot

- (PresenterSlideInfo*)currentInfo {
    os_unfair_lock_lock(&_lock);
    PresenterSlideInfo* info = [[PresenterSlideInfo alloc] initWithHandle:_handle];
    os_unfair_lock_unlock(&_lock);
    return info;
}

- (NSString*)slideTitleAt:(NSInteger)index {
    os_unfair_lock_lock(&_lock);
    PresenterSlideData slideInfo = presenter_slide_info(_handle, (int)index);
    NSString* title = @(slideInfo.title);
    os_unfair_lock_unlock(&_lock);
    return title;
}

// ---------------------------------------------------------------------------
// Theme

- (NSInteger)themeCount {
    return presenter_theme_count(_handle);
}

- (NSString*)themeNameAt:(NSInteger)index {
    return @(presenter_theme_name(_handle, (int)index));
}

- (NSInteger)currentTheme {
    return presenter_current_theme(_handle);
}

- (void)setCurrentTheme:(NSInteger)index {
    os_unfair_lock_lock(&_lock);
    presenter_set_theme(_handle, (int)index);
    os_unfair_lock_unlock(&_lock);
}

// ---------------------------------------------------------------------------
// Pixel-buffer rendering

- (BOOL)renderSlideIntoPixels:(uint8_t*)pixels
                        width:(int)width
                       height:(int)height
                       stride:(int)stride {
    os_unfair_lock_lock(&_lock);
    int ok = presenter_render_slide_rgba(_handle, pixels, width, height, stride);
    os_unfair_lock_unlock(&_lock);
    return (BOOL)ok;
}

- (BOOL)renderPresenterViewIntoPixels:(uint8_t*)pixels
                                width:(int)width
                               height:(int)height
                               stride:(int)stride {
    os_unfair_lock_lock(&_lock);
    int ok = presenter_render_presenter_rgba(_handle, pixels, width, height, stride);
    os_unfair_lock_unlock(&_lock);
    return (BOOL)ok;
}

@end
