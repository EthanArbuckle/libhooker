//
//  main.m
//  libhooker-test
//
//  Created by CoolStar on 8/17/19.
//  Copyright © 2019 CoolStar. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"
#import <sys/utsname.h>
#import "../unittest/unittest.c"

static int (*_test_LBHookMessage_orig)(AppDelegate *, SEL);
static int _test_LBHookMessage_replacement(AppDelegate *self, SEL _cmd) {
    return _test_LBHookMessage_orig(self, _cmd) + 20;
}
static kern_return_t test_LBHookMessage(void) {
    
    int expectedOriginalValue = [[AppDelegate2 new] test];
    int expectedReplacementValue = expectedOriginalValue + 20;
    
    int result = LBHookMessage([AppDelegate2 class], @selector(test), (IMP)&_test_LBHookMessage_replacement, (IMP *)&_test_LBHookMessage_orig);
    ASSERT(result == LIBHOOKER_OK, "LBHookMessage() failed");
    ASSERT(_test_LBHookMessage_orig != NULL, "Orig fp is NULL");
    
    ASSERT([[AppDelegate2 new] test] == expectedReplacementValue, "Hooked method did not return the expected value");
    ASSERT(_test_LBHookMessage_orig([AppDelegate2 new], @selector(test)) == expectedOriginalValue, "Orig fp didn't return the expected value");
    
    return KERN_SUCCESS;
}

/*void testReverseEngineering(){
 uint64_t lrAddr = 0;
 asm("mov %0, x30"
 : "=r"(lrAddr)
 : "0"(lrAddr));
 printf("Executable Name: %s (should be self)\n", LHAddrExecutable(lrAddr));
 
 printf("Executable Name: %s (should be libsystem)\n", LHAddrExecutable((void *)dlsym(RTLD_DEFAULT, "uname")));
 }*/

int main(int argc, char * argv[]) {
    @autoreleasepool {
        struct utsname kern;
        uname(&kern);
        
        printf("kern: %s\n", kern.version);
        
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
        if (ptrauth_sign_unauthenticated((void *)0x12345, ptrauth_key_asda, 0) == (void *)0x12345){
            printf("Running without PAC\n");
        } else {
            printf("Running with PAC\n");
        }
#pragma GCC diagnostic pop

        printCSOps();
        RUN_TEST(test_LBHookMessage);
        RUN_TEST(test_LHOpenImage);
        RUN_TEST(test_LHFindSymbols__single_symbol_and_invoke);
        RUN_TEST(test_LHFindSymbols__single_symbol);
        RUN_TEST(test_LHFindSymbols__multiple_symbols);
        RUN_TEST(test_getDyldCacheHeader);
        
        RUN_TEST(test_LHPatchMemory_single_patch);
        RUN_TEST(test_LHPatchMemory_multiple_patches);
        
        RUN_TEST(test_LHFunctionHook__single_function);
        RUN_TEST(test_LHFunctionHook__double_hook);
        RUN_TEST(test_LHFunctionHook__unlink);
        RUN_TEST(test_LHFunctionHook__4_instruction_function);
        RUN_TEST(test_LHFunctionHook__CFPreferencesGetAppIntegerValue);
        RUN_TEST_NO_RET(test_LHFunctionHook__UIApplicationInitialize);
        
        //testReverseEngineering();
        
        /*uint32_t opcode = *((uint32_t *)&CFBundleCopyResourceURL);
         free(handle_cbz((uint64_t)&CFBundleCopyResourceURL, opcode));
         free(handle_tbz((uint64_t)&CFBundleCopyResourceURL, 0x361001e0));
         free(handle_bCond((uint64_t)&CFBundleCopyResourceURL, 0x540001eb));
         free(handle_ldr((uint64_t)&CFBundleCopyResourceURL, 0x581fffea));
         free(handle_ldr((uint64_t)&CFBundleCopyResourceURL, 0x981fffea));*/
        
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate2 class]));
    }
}
