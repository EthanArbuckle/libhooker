#include <objc/runtime.h>
#include "libhooker.h"
#include "libblackjack.h"
#include <os/log.h>
#include <stdlib.h>
#include <mach-o/dyld.h>
#include "dyldSyms.h"
#include <dlfcn.h>
#include <string.h>

void *MSGetImageByName(const char *filename) {
    // Not returning LH structures here -- these handles should be usable by dyld APIs
    return dlopen(filename, RTLD_NOW);
}

void *MSFindSymbol(void *image, const char *name) {
    if (!image){
        os_log_error(OS_LOG_DEFAULT, "MSFindSymbol: NULL image specified. You should be ashamed of yourself.\n");
        os_log_error(OS_LOG_DEFAULT, "MSFindSymbol: Also btw. Stop using this Substrate compatibility shim.\n");

        for (uint32_t i = 0; i < _dyld_image_count(); i++) {
            const char *path = _dyld_get_image_name(i);
            struct libhooker_image *lh_image = LHOpenImage(path);
            if (!lh_image) {
                continue;
            }
            
            void *found_symbol = MSFindSymbol(lh_image->dyldHandle, name);
            LHCloseImage(lh_image);
            if (found_symbol) {
                return found_symbol;
            }
        }
        return NULL;
    }
    
    /*
    // Not ideal, but LH's APIs expect to be given a path, and this substrate API gets
    // a dlopen() handle
    // For it's just going to use dlsym().
     */
    void *found_symbol = dlsym(image,  name);
    if (found_symbol == NULL && strncmp(name, "_", 1) == 0) {
        // If the first call to dlsym() failed, and the symbol name starts with an underscore,
        // remove the leading underscore and attempt to call dlsym() again
        const char *symbol_without_underscore = name + 1;
        found_symbol = dlsym(image, symbol_without_underscore);
    }

    return found_symbol;
    
    /*
    void *address_of_mh = dlsym(image, "__mh_execute_header");
    
    Dl_info info = {};
    if (dladdr(address_of_mh, &info) == 0) {
        return NULL;
    }
    
    const char *image_path = info.dli_fname;
    if (image_path == NULL || strlen(image_path) < 1) {
        return NULL;
    }

    struct libhooker_image *lh_image = LHOpenImage(image_path);
    if (!lh_image) {
        return NULL;
    }
    
    void *found_symbol = NULL;
    LHFindSymbols(lh_image, &name, &found_symbol, 1);
    return found_symbol;
    */
}

void MSHookFunction(void *symbol, void *replace, void **result) {
	if (symbol != NULL){
        struct LHFunctionHook hooks[] = {
            {symbol, replace, result}
        };
        LHHookFunctions(hooks, 1);
	}
}

void MSHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result) {
    LBHookMessage(_class, sel, imp, result);
}

// i don't think anyone uses this function anymore, but it's here for completeness
void MSHookClassPair(Class _class, Class hook, Class old) {
    unsigned int n_methods = 0;
    Method *hooks = class_copyMethodList(hook, &n_methods);
    
    for (unsigned int i = 0; i < n_methods; ++i) {
        SEL selector = method_getName(hooks[i]);
        const char *what = method_getTypeEncoding(hooks[i]);
        
        Method old_mptr = class_getInstanceMethod(old, selector);
        Method cls_mptr = class_getInstanceMethod(_class, selector);
        
        if (cls_mptr) {
            class_addMethod(old, selector, method_getImplementation(hooks[i]), what);
            method_exchangeImplementations(cls_mptr, old_mptr);
        } else {
            class_addMethod(_class, selector, method_getImplementation(hooks[i]), what);
        }
    }
    
    free(hooks);
}

void MSHookMemory(void *target, const void *data, size_t size){
    struct LHMemoryPatch patch[] = {
        {target, data, size}
    };
    LHPatchMemory(patch, 1);
}
