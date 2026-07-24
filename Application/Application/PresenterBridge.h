/**
 * PresenterBridge.h — Objective-C wrapper around the flat C presenter API.
 *
 * Swift cannot call C++ directly.  This ObjC class forms the bridge:
 *   Swift  ←→  PresenterBridge (ObjC)  ←→  presenter_api.h (C)  ←→  C++ core
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** Immutable snapshot of presenter state, safe to consume on the main actor. */
@interface PresenterSlideInfo : NSObject
@property (nonatomic, readonly) NSInteger slideIndex;   ///< 0-based
@property (nonatomic, readonly) NSInteger slideCount;
@property (nonatomic, readonly) NSString* slideLabel;   ///< "3 / 12"
@property (nonatomic, readonly) NSString* notes;
@property (nonatomic, readonly) NSString* presentationTitle;
@property (nonatomic, readonly) BOOL canGoNext;
@property (nonatomic, readonly) BOOL canGoPrev;
@end

/** Wraps a PresenterHandle; all methods are thread-safe (internal serial queue). */
@interface PresenterBridge : NSObject

/** Returns nil if the file cannot be parsed or fonts fail to load. */
- (nullable instancetype)initWithSlidesPath:(NSString*)slidesPath;

- (void)dealloc;

// Navigation
- (void)next;
- (void)prev;
- (void)goTo:(NSInteger)index;

// Current state snapshot (safe to call from any thread)
- (PresenterSlideInfo*)currentInfo;
- (NSString*)slideTitleAt:(NSInteger)index;

// Theme
@property (nonatomic, readonly) NSInteger themeCount;
- (NSString*)themeNameAt:(NSInteger)index;
@property (nonatomic) NSInteger currentTheme;

/**
 * Render the audience slide view into a pre-allocated RGBA8888 buffer.
 * stride is the row stride in bytes (typically width*4).
 * Returns YES on success.
 */
- (BOOL)renderSlideIntoPixels:(uint8_t*)pixels
                        width:(int)width
                       height:(int)height
                       stride:(int)stride;

/**
 * Render the presenter view (current + next + notes) into a pre-allocated
 * RGBA8888 buffer.  Returns YES on success.
 */
- (BOOL)renderPresenterViewIntoPixels:(uint8_t*)pixels
                                width:(int)width
                               height:(int)height
                               stride:(int)stride;

@end

NS_ASSUME_NONNULL_END
