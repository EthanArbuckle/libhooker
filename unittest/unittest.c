#import <dlfcn.h>
#import <ptrauth.h>
#import <unistd.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <mach-o/dyld.h>
#import <CoreFoundation/CoreFoundation.h>

#ifdef THEOS
#import "../libhooker/dyld/dyldSyms.h"
#import "../libhooker/mem/writeMem.h"
#import "../libhooker/as-aarch64/as-aarch64.h"
#import "../libhooker/disas-aarch64/disas-aarch64.h"
#import "../libhooker/func/funcHook.h"
#import "../libhooker/include/libhooker.h"
#import "../libblackjack/include/libblackjack.h"
#else
#import "dyldSyms.h"
#import "writeMem.h"
#import "as-aarch64.h"
#import "disas-aarch64.h"
#import "funcHook.h"
#import "libhooker.h"
#import "libblackjack.h"
#endif

#define ASSERT(condition, message) \
do { \
if (!(condition)) { \
printf("%s:%d: %s: Test failed: %s\n", \
__FILE__, __LINE__, __func__, message); \
return -1; \
} \
} while (0)


#define ASSERT_NO_RET(condition, message) \
do { \
if (!(condition)) { \
printf("%s:%d: %s: Test failed: %s\n", \
__FILE__, __LINE__, __func__, message); \
return; \
} \
} while (0)

#define RUN_TEST_NO_RET(test) do { \
printf("Running test: %s\n", #test); \
test(); \
} while (0)

#define RUN_TEST(test) do { \
printf("Running test: %s\n", #test); \
if (test() == KERN_SUCCESS) printf("  PASSED\n"); \
} while (0)

#define    CS_OPS_STATUS        0    /* return status */
int csops(pid_t pid, unsigned int  ops, void * useraddr, size_t usersize);

void printCSOps(void){
    uint32_t ops;
    int retval = csops(getpid(), CS_OPS_STATUS, &ops, sizeof(uint32_t));
    printf("retval: %d, ops: 0x%x\n", retval, ops);
}

static kern_return_t test_LHOpenImage(void) {
    
    void *expectedHandle = dlopen("/usr/lib/swift/libswiftDemangle.dylib", RTLD_NOW);
    ASSERT(expectedHandle, "dlopen returned NULL");
    
    const struct mach_header *expectedHeader = NULL;
    uintptr_t expectedSlide = -1;
    for (int i = 0; i < _dyld_image_count(); i++) {
        if (strstr(_dyld_get_image_name(i), "libswiftDemangle.dylib")) {
            expectedHeader = _dyld_get_image_header(i);
            expectedSlide = _dyld_get_image_vmaddr_slide(i);
            break;
        }
    }
    ASSERT(expectedHeader, "Could not find mach_header with _dyld_get_image_header()");
    
    struct libhooker_image *lhHandle = LHOpenImage("/usr/lib/swift/libswiftDemangle.dylib");
    ASSERT(lhHandle, "LHOpenImage() returned NULL");
    
    int cond1 = lhHandle->dyldHandle == expectedHandle;
    int cond2 = lhHandle->imageHeader == expectedHeader;
    int cond3 = lhHandle->slide == expectedSlide;
    
    LHCloseImage(lhHandle);
    
    ASSERT(cond1, "LH's dyldHandle doesn't match dlopen's");
    ASSERT(cond2, "LH's imageHeader doesn't match _dyld_get_image_header()");
    ASSERT(cond3, "LH's slide doesn't match _dyld_get_image_vmaddr_slide");
    
    return KERN_SUCCESS;
}

static kern_return_t test_LHFindSymbols__single_symbol_and_invoke(void) {
    
    const char *symbol_names[1] = {
        "_swift_demangle_getDemangledName"
    };
    void *symbol_pointers[1];
    
    struct libhooker_image *lhHandle = LHOpenImage("/usr/lib/swift/libswiftDemangle.dylib");
    ASSERT(lhHandle, "LHOpenImage() returned NULL");
    
    int success = LHFindSymbols(lhHandle, symbol_names, symbol_pointers, 1);
    
    ASSERT(success, "LHFindSymbols() failed");
    ASSERT(symbol_pointers[0], "LHFindSymbols failed to find _swift_demangle_getDemangledName");
    
    // Make sure returned symbols are callable
    char demangled[65];
    ((size_t (*)(const char *, char *, size_t))symbol_pointers[0])("_TMaC17find_the_treasure15YD_Secret_Class", demangled, 65);
    ASSERT(strcmp(demangled, "type metadata accessor for find_the_treasure.YD_Secret_Class") == 0, "Invoked function has unexpected return value");
    
    LHCloseImage(lhHandle);
    return KERN_SUCCESS;
}

static kern_return_t test_LHFindSymbols__single_symbol(void) {
    
    const char *symbol_names[1] = {
        "_kCTUIFontTextStyleBody"
    };
    void *symbol_pointers[1];
    
    struct libhooker_image *lhHandle = LHOpenImage("/System/Library/Frameworks/CoreText.framework/CoreText");
    ASSERT(lhHandle, "LHOpenImage() returned NULL");
    
    int success = LHFindSymbols(lhHandle, symbol_names, symbol_pointers, 1);
    LHCloseImage(lhHandle);
    
    ASSERT(success, "LHFindSymbols() failed");
    ASSERT(symbol_pointers[0], "LHFindSymbols failed to find _CTFontLogSuboptimalRequest");
    
    return KERN_SUCCESS;
}

static kern_return_t test_LHFindSymbols__multiple_symbols(void) {
    
    const char *symbol_names[7] = {
        "_abort",
        "__libkernel_memmove",
        "__libkernel_strlen",
        "__libkernel_bzero",
        "__libkernel_memset",
        "__libkernel_strcpy",
        "__libkernel_strlcpy"
    };
    void *symbol_pointers[7];
    
    struct libhooker_image *lhHandle = LHOpenImage("/usr/lib/system/libsystem_kernel.dylib");
    ASSERT(lhHandle, "LHOpenImage() returned NULL");
    
    int success = LHFindSymbols(lhHandle, symbol_names, symbol_pointers, 7);
    LHCloseImage(lhHandle);
    
    ASSERT(success, "LHFindSymbols() failed");
    
    for (int i = 0; i < 6; i++) {
        ASSERT(symbol_pointers[i] != NULL, "LHFindSymbols failed to find one of the provided symbol names");
    }
    
    return KERN_SUCCESS;
}

kern_return_t test_getDyldCacheHeader(void) {
    ASSERT(getDyldCacheHeader(), "getDyldCacheHeader() returned NULL");
    return KERN_SUCCESS;
}

__attribute__((aligned(0x4000)))
int _test_LHPatchMemory_single_patch_target(void) { return -10; }
static kern_return_t test_LHPatchMemory_single_patch(void) {
    
    int expectedOriginalValue = -10;
    int expectedReplacementValue = 5;
    
    char replacementInstrs[] = {
        0xa0, 0x00, 0x80, 0xd2, //mov x0, #5
        0xc0, 0x03, 0x5f, 0xd6  //ret
    };
    
    ASSERT(expectedOriginalValue == _test_LHPatchMemory_single_patch_target(), "Function didn't have expected retval before memory patch");
    
    struct LHMemoryPatch patch = {
        ptrauth_strip(&_test_LHPatchMemory_single_patch_target, ptrauth_key_asia), replacementInstrs, 8 * sizeof(char)
    };
    
    int successfulPatches = LHPatchMemory(&patch, 1);
    
    ASSERT(successfulPatches == 1, "LHPatchMemory() failed");
    ASSERT(expectedReplacementValue == _test_LHPatchMemory_single_patch_target(), "Function didn't have expected retval after memory patch");
    
    return KERN_SUCCESS;
}

__attribute__((aligned(0x4000)))
int _test_LHPatchMemory_multiple_patches_target(void) { return 45; }
static kern_return_t test_LHPatchMemory_multiple_patches(void) {
    
    int expectedOriginalValue_1 = 45;
    int expectedReplacementValue_1 = 10;
    int expectedReplacementValue_2 = 3;
    
    char replacementInstrs_1[] = {
        0x40, 0x01, 0x80, 0xd2, // mov x0, #10
        0xc0, 0x03, 0x5f, 0xd6  // ret
    };
    
    char replacementInstrs_2[] = {
        0x60, 0x00, 0x80, 0xd2, // mov x0, #3
        0x01, 0xcf, 0x8a, 0x52, // movz x1, #0x5678
        0x81, 0x46, 0xa2, 0x72, // movk w1, #0x1234, lsl 16
        0x41, 0x00, 0x00, 0xb9, // str x1, [x2]
        0xc0, 0x03, 0x5f, 0xd6  // ret
    };
    
    ASSERT(expectedOriginalValue_1 == _test_LHPatchMemory_multiple_patches_target(), "Function didn't have expected retval before memory patch");
    
    struct LHMemoryPatch patches[2] = {
        {ptrauth_strip(&_test_LHPatchMemory_multiple_patches_target, ptrauth_key_asia), replacementInstrs_1, 8 * sizeof(char)},
        {csops, replacementInstrs_2, 20 * sizeof(char)}
    };
    
    int successfulPatches = LHPatchMemory(patches, 2);
    
    ASSERT(successfulPatches == 2, "LHPatchMemory() failed");
    ASSERT(expectedReplacementValue_1 == _test_LHPatchMemory_multiple_patches_target(), "Function didn't have expected retval after memory patch");
    
    uint32_t ops;
    int replacementValue_2 = csops(getpid(), CS_OPS_STATUS, &ops, sizeof(uint32_t));
    ASSERT(replacementValue_2 == expectedReplacementValue_2, "Function didn't have expected retval after memory patch");
    ASSERT(ops == 0x12345678, "_csops ops didn't have expected value after memory patch");
    
    return KERN_SUCCESS;
}

__attribute__((aligned(0x4000)))
int (*_test_LHFunctionHook__single_function_target_orig)(void);
int _test_LHFunctionHook__single_function_target(void) { getpid(); return 1; }
int _test_LHFunctionHook__single_function_replacement(void) { getpid(); return 2; }
static kern_return_t test_LHFunctionHook__single_function(void) {
    
    int expectedOriginalValue = _test_LHFunctionHook__single_function_target();
    int expectedReplacementValue = _test_LHFunctionHook__single_function_replacement();
    ASSERT(expectedOriginalValue != expectedReplacementValue, "The target and replacement test functions have the same return value");
    
    struct LHFunctionHook hooks[] = {
        {&_test_LHFunctionHook__single_function_target, &_test_LHFunctionHook__single_function_replacement, &_test_LHFunctionHook__single_function_target_orig}
    };
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed");
    ASSERT(_test_LHFunctionHook__single_function_target_orig != NULL, "Original function ptr is null");
    
    ASSERT(_test_LHFunctionHook__single_function_target() == expectedReplacementValue, "Function didn't return the expected value");
    ASSERT(_test_LHFunctionHook__single_function_target_orig() == expectedOriginalValue, "The orig fp didn't return the expected value");
    
    return KERN_SUCCESS;
}

__attribute__((aligned(0x4000)))
int (*_test_LHFunctionHook__double_hook_target1_orig)(void);
int _test_LHFunctionHook__double_hook_target(void) { getpid(); return 3; }
int _test_LHFunctionHook__double_hook_replacement1(void) { getpid(); return 4; }
int _test_LHFunctionHook__double_hook_replacement2(void) { getpid(); return 5; }
static kern_return_t test_LHFunctionHook__double_hook(void) {
    
    int expectedOriginalValue = _test_LHFunctionHook__double_hook_target();
    int expectedReplacementValue1 = _test_LHFunctionHook__double_hook_replacement1();
    int expectedReplacementValue2 = _test_LHFunctionHook__double_hook_replacement2();
    ASSERT(expectedOriginalValue != expectedReplacementValue1 && expectedReplacementValue1 != expectedReplacementValue2 && expectedOriginalValue != expectedReplacementValue2, "Test functions have the same return value");
    
    struct LHFunctionHook hooks[] = {
        {&_test_LHFunctionHook__double_hook_target, &_test_LHFunctionHook__double_hook_replacement1, &_test_LHFunctionHook__double_hook_target1_orig}
    };
    
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed on first hook");
    ASSERT(_test_LHFunctionHook__double_hook_target1_orig != NULL, "Original function ptr is null after first hook");
    
    ASSERT(_test_LHFunctionHook__double_hook_target() == expectedReplacementValue1, "Function didn't return the expected value after first hook");
    ASSERT(_test_LHFunctionHook__double_hook_target1_orig() == expectedOriginalValue, "The orig fp didn't return the expected value after the first hook");
    
    struct LHFunctionHook hooks2[] = {
        {&_test_LHFunctionHook__double_hook_target, &_test_LHFunctionHook__double_hook_replacement2, &_test_LHFunctionHook__double_hook_target1_orig}
    };
    
    successfulHooks = LHHookFunctions(hooks2, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed on second hook");
    ASSERT(_test_LHFunctionHook__double_hook_target1_orig != NULL, "Original function ptr is null after second hook");
    
    ASSERT(_test_LHFunctionHook__double_hook_target() == expectedReplacementValue2, "Function didn't return the expected value after second hook");
    ASSERT(_test_LHFunctionHook__double_hook_target1_orig() == expectedReplacementValue1, "The orig fp didn't return the expected value after the second hook");
    
    return KERN_SUCCESS;
}

__attribute__((aligned(0x4000)))
static int (*_test_LHFunctionHook__unlink_orig)(const char *);
static int _test_LHFunctionHook__unlink_replacement(const char *path) { getpid(); return 14; }
static kern_return_t test_LHFunctionHook__unlink(void) {
    
    // Someone mentioned that calling orig on hooked unlink() can be problematic
    struct LHFunctionHook hooks[] = {
        {&unlink, &_test_LHFunctionHook__unlink_replacement, &_test_LHFunctionHook__unlink_orig}
    };
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed");
    ASSERT(_test_LHFunctionHook__unlink_orig != NULL, "Original function ptr is null");
    
    NSString *tmpTestFile = [NSTemporaryDirectory() stringByAppendingPathComponent:@"lh-tmp-test"];
    [[NSFileManager defaultManager] createFileAtPath:tmpTestFile contents:nil attributes:nil];
    
    ASSERT(_test_LHFunctionHook__unlink_orig(tmpTestFile.UTF8String) == 0, "The orig fp didn't return the expected value");
    
    ASSERT([[NSFileManager defaultManager] fileExistsAtPath:tmpTestFile] == NO, "unlink_orig did not delete the file");
    [[NSFileManager defaultManager] removeItemAtPath:tmpTestFile error:nil];
    
    return KERN_SUCCESS;
}


int (*_test_LHFunctionHook__4_instruction_function_target_orig)(int arg1, int arg2);
__attribute__((naked)) int _test_LHFunctionHook__4_instruction_function_target(int arg1, int arg2) {
    __asm__ volatile (
                      "cbz x0, #0x8\n"      // if arg1 is 0, jump to the nop, skipping the add
                      "add x0, x1, x0\n"
                      "nop\n"
                      "ret"
                      );
}
__attribute__((aligned(0x4000))) int _test_LHFunctionHook__4_instruction_function_replacement(int arg1, int arg2) {
    return _test_LHFunctionHook__4_instruction_function_target_orig(arg1, arg2) + 1;
}
static kern_return_t test_LHFunctionHook__4_instruction_function(void) {
    
    int expectedOriginalValue1 = 5;
    int expectedReplacementValue1 = expectedOriginalValue1 + 1;
    int expectedOriginalValue2 = 0;
    int expectedReplacementValue2 = expectedOriginalValue2 + 1;
    
    ASSERT(_test_LHFunctionHook__4_instruction_function_target(3, 2) == expectedOriginalValue1, "Pre-hook target function didn't return the expected value (3 + 2)");
    ASSERT(_test_LHFunctionHook__4_instruction_function_target(0, 2) == expectedOriginalValue2, "Pre-hook target function didn't return the expected value (0 + 2)");
    
    struct LHFunctionHook hooks[] = {
        {&_test_LHFunctionHook__4_instruction_function_target, &_test_LHFunctionHook__4_instruction_function_replacement, &_test_LHFunctionHook__4_instruction_function_target_orig}
    };
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed");
    ASSERT(_test_LHFunctionHook__4_instruction_function_target_orig != NULL, "Original function ptr is null");
    
    ASSERT(_test_LHFunctionHook__4_instruction_function_target(3, 2) == expectedReplacementValue1, "Post-hook function didn't return the expected value (3 + 2)");
    ASSERT(_test_LHFunctionHook__4_instruction_function_target(0, 2) == expectedReplacementValue2, "Post-hook function didn't return the expected value (0 + 2)");
    
    ASSERT(_test_LHFunctionHook__4_instruction_function_target_orig(3, 2) == expectedOriginalValue1, "The orig fp didn't return the expected value (3 + 2)");
    ASSERT(_test_LHFunctionHook__4_instruction_function_target_orig(0, 2) == expectedOriginalValue2, "The orig fp didn't return the expected value (0 + 2)");
    
    return KERN_SUCCESS;
}

CFIndex (*_test_LHFunctionHook__CFPreferencesGetAppIntegerValue_orig)(CFStringRef, CFStringRef, Boolean *);
int _test_LHFunctionHook__CFPreferencesGetAppIntegerValue_replacement(CFStringRef key, CFStringRef applicationID, Boolean *keyExistsAndHasValidFormat) {
    return 1234;
}
static kern_return_t test_LHFunctionHook__CFPreferencesGetAppIntegerValue(void) {
    
    Boolean keyExistsAndHasValidFormat;
    CFIndex expectedOriginalValue = CFPreferencesGetAppIntegerValue(CFSTR("canvas_width"), CFSTR("com.apple.iokit.IOMobileGraphicsFamily"), &keyExistsAndHasValidFormat);
    CFIndex expectedReplacementValue = _test_LHFunctionHook__CFPreferencesGetAppIntegerValue_replacement(CFSTR("canvas_width"), CFSTR("com.apple.iokit.IOMobileGraphicsFamily"), &keyExistsAndHasValidFormat);
    ASSERT(expectedOriginalValue != expectedReplacementValue, "The target and replacement test functions have the same return value");
    
    struct LHFunctionHook hooks[] = {
        {&CFPreferencesGetAppIntegerValue, &_test_LHFunctionHook__CFPreferencesGetAppIntegerValue_replacement, &_test_LHFunctionHook__CFPreferencesGetAppIntegerValue_orig}
    };
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed");
    ASSERT(_test_LHFunctionHook__CFPreferencesGetAppIntegerValue_orig != NULL, "Original function ptr is null");
    
    ASSERT(CFPreferencesGetAppIntegerValue(CFSTR("canvas_width"), CFSTR("com.apple.iokit.IOMobileGraphicsFamily"), &keyExistsAndHasValidFormat) == expectedReplacementValue, "Function didn't return the expected value");
    ASSERT(_test_LHFunctionHook__CFPreferencesGetAppIntegerValue_orig(CFSTR("canvas_width"), CFSTR("com.apple.iokit.IOMobileGraphicsFamily"), &keyExistsAndHasValidFormat) == expectedOriginalValue, "The orig fp didn't return the expected value");
    
    return KERN_SUCCESS;
}

static int _test_LHFunctionHook__UIApplicationInitialized_did_intercept = 0;
static dispatch_semaphore_t _test_LHFunctionHook__UIApplicationInitialized_did_intercept_sem;
size_t (*_test_LHFunctionHook__UIApplicationInitialize_orig)(void);
static size_t _test_LHFunctionHook__UIApplicationInitialize_replacement(const char *path) {
    _test_LHFunctionHook__UIApplicationInitialized_did_intercept = 1;
    dispatch_semaphore_signal(_test_LHFunctionHook__UIApplicationInitialized_did_intercept_sem);
    return _test_LHFunctionHook__UIApplicationInitialize_orig();
}
static void test_LHFunctionHook__UIApplicationInitialize(void) {
    
    _test_LHFunctionHook__UIApplicationInitialized_did_intercept_sem = dispatch_semaphore_create(0);
    
    const char *symbol_names[1] = {"_UIApplicationInitialize"};
    void *symbol_pointers[1];
    struct libhooker_image *lhHandle = LHOpenImage("/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore");
    
    LHFindSymbols(lhHandle, symbol_names, symbol_pointers, 1);
    LHCloseImage(lhHandle);
    
    struct LHFunctionHook hooks[] = {
        {symbol_pointers[0], &_test_LHFunctionHook__UIApplicationInitialize_replacement, &_test_LHFunctionHook__UIApplicationInitialize_orig},
    };
    
    LHHookFunctions(hooks, 1);
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        dispatch_semaphore_wait(_test_LHFunctionHook__UIApplicationInitialized_did_intercept_sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)));
        ASSERT_NO_RET(_test_LHFunctionHook__UIApplicationInitialized_did_intercept == 1, "Failed to intercept invocation to UIApplicationInitialize");
        printf("  PASSED\n");
    });
}

__attribute__((aligned(0x4000))) //separate page
const char *(*origLHStrError)(int);
const char *newLHStrError(int err){
    if (err == -30){
        return "Hooked ourselves successfully!";
    }
    return origLHStrError(err);
}

__attribute__((aligned(0x4000))) //separate page
__unused static void testLHHook(void){
    printf("Initial Error: %s\n", LHStrError(-30));
    struct LHFunctionHook hooks[] = {
        {&LHStrError, &newLHStrError, &origLHStrError}
    };
    LHHookFunctions(hooks, 1);
    
    printf("New Error: %s\n", LHStrError(-30));
}

extern void *MSGetImageByName(const char *filename);
kern_return_t test_shim__substrate_MSGetImageByName(void) {
    
    void *handle = MSGetImageByName("/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore");
    ASSERT(handle != NULL, "MSGetImageByName() returned NULL");
    
    return KERN_SUCCESS;
}

extern void *MSFindSymbol(void *image, const char *name);
kern_return_t test_shim__substrate_MSFindSymbol(void) {
    
    void *handle = dlopen("/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore", RTLD_NOW);
    void *symbol = MSFindSymbol(handle, "_UIApplicationInitialize");
    ASSERT(symbol != NULL, "MSFindSymbol() returned NULL");
    
    return KERN_SUCCESS;
}

extern void MSHookFunction(void *symbol, void *replace, void **result);
__attribute__((aligned(0x4000)))
int (*_test_shim__substrate_MSHookFunction_orig)(void);
int _test_shim__substrate_MSHookFunction_target(void) { getpid(); return KERN_FAILURE; }
int _test_shim__substrate_MSHookFunction_replacement(void) { return KERN_SUCCESS; }
kern_return_t test_shim__substrate_MSHookFunction(void) {
    
    int expectedOriginalValue = _test_shim__substrate_MSHookFunction_target();
    int expectedReplacementValue = _test_shim__substrate_MSHookFunction_replacement();
    ASSERT(expectedOriginalValue != expectedReplacementValue, "The target and replacement test functions have the same return value");
    
    struct LHFunctionHook hooks[] = {
        {&_test_shim__substrate_MSHookFunction_target, &_test_shim__substrate_MSHookFunction_replacement, &_test_shim__substrate_MSHookFunction_orig}
    };
    int successfulHooks = LHHookFunctions(hooks, 1);
    ASSERT(successfulHooks == 1, "LHHookFunctions failed");
    ASSERT(_test_shim__substrate_MSHookFunction_orig != NULL, "Original function ptr is null");
    
    ASSERT(_test_shim__substrate_MSHookFunction_target() == expectedReplacementValue, "Post-hook function didn't return the expected value");
    ASSERT(_test_shim__substrate_MSHookFunction_orig() == expectedOriginalValue, "The orig fp didn't return the expected value");
    
    return KERN_SUCCESS;
}

extern void MSHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result);
int (*_test_shim__substrate_MSHookMessageEx_orig)(void);
kern_return_t test_shim__substrate_MSHookMessageEx(void) {
    
    // Given a class called MSHookMessageTestTarget
    Class MSHookMessageTestTargetClass = objc_allocateClassPair(objc_getClass("NSObject"), "MSHookMessageTestTarget", 0);
    
    // And MSHookMessageTestTarget implements a class method called hookMethodTarget
    // which always returns KERN_FAILURE
    SEL hookMethodTarget = sel_registerName("hookMethodTarget");
    IMP hookMethodTargetRealImp = imp_implementationWithBlock(^kern_return_t(id _self, SEL _cmd) {
        return KERN_FAILURE;
    });
    
    objc_registerClassPair(MSHookMessageTestTargetClass);
    Class MSHookMessageTestTargetMetaClass = object_getClass(MSHookMessageTestTargetClass);
    BOOL addedMethod = class_addMethod(MSHookMessageTestTargetMetaClass, hookMethodTarget, hookMethodTargetRealImp, "i@:");
    ASSERT(addedMethod, "Failed to build MSHookMessageTestTarget class");
    
    // When I call +[MSHookMessageTestTarget hookMethodTarget], KERN_FAILURE is returned
    ASSERT(((kern_return_t (*)(id, SEL))objc_msgSend)(MSHookMessageTestTargetClass, hookMethodTarget) == KERN_FAILURE, "Pre-hook hookMethodTarget didn't return the expected value");
    
    // And given a replacement IMP that always returns KERN_SUCCESS
    IMP replacementImp = imp_implementationWithBlock(^kern_return_t (id _self, SEL _cmd) {
        return KERN_SUCCESS;
    });
    
    // When I hook the method using the MSHookMessageEx()
    MSHookMessageEx(MSHookMessageTestTargetMetaClass, hookMethodTarget, replacementImp, (IMP *)&_test_shim__substrate_MSHookMessageEx_orig);
    // The orig pointer has been populated
    ASSERT(_test_shim__substrate_MSHookMessageEx_orig != NULL, "Original function ptr is NULL");
    
    // And calling the method now returns the replacement IMP's value
    ASSERT(((kern_return_t (*)(id, SEL))objc_msgSend)(MSHookMessageTestTargetClass, hookMethodTarget) == KERN_SUCCESS, "Post-hook function didn't return the expected value");
    
    // And calling the original fp returns the method's original pre-hook value
    ASSERT(_test_shim__substrate_MSHookMessageEx_orig() == KERN_FAILURE, "The orig fp didn't return the expected value");
    
    return KERN_SUCCESS;
}
