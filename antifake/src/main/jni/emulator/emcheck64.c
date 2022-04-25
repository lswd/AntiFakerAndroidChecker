#include <jni.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/mman.h>
#include <android/log.h>
#include<fcntl.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_ERROR,"lishang",__VA_ARGS__)
#define PROT PROT_EXEC|PROT_WRITE|PROT_READ
//这里其实主要是检测是不是在x86或者在arm上运行


const int handledSignals[] = {
        SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS
};
const int handledSignalsNum = sizeof(handledSignals) / sizeof(handledSignals[0]);
struct sigaction old_handlers[5];

void my_sigaction(int signal, siginfo_t *info, void *reserved) {
    LOGI("Crash detect  signal %d",signal);
    exit(0);
}

int load(JNIEnv *env) {
    struct sigaction handler;
    memset(&handler, 0, sizeof(sigaction));
    handler.sa_sigaction = my_sigaction;
    handler.sa_flags = SA_RESETHAND;
    int i = 0;
    for ( ; i < handledSignalsNum; ++i) {
        sigaction(handledSignals[i], &handler, &old_handlers[i]);
    }
    return 1;
}

int a = -1;
int (*asmcheck)(void);

jboolean JNICALL detect (JNIEnv *env ,jclass jclass1) {
    load(env);
char code[] =
	 	"\xff\xc3\x00\xd1"//	sub	sp, sp, #0x30
	 	"\xfd\x7b\x02\xa9"//	x29, x30, [sp,#32]
	 	"\x02\x00\x80\xd2"//	x2, #0x0
	 	"\x00\x00\x80\xd2"//	mov	x0, #0x0
	 	"\x42\x04\x00\x91"//	add	x2, x2, #0x1
	 	"\xe3\xff\xff\x10"//	adr	x3, 18 <smc>
	 	"\x61\x00\x40\xf9"//	ldr	x1, [x3]
	 	"\x00\x04\x00\x91"//	add	x0, x0, #0x1
	 	"\xe3\xff\xff\x10"//	adr	x3, 24 <code>
	  	"\x61\x00\x00\xf9"//	str	x1, [x3]
	 	"\x1f\x28\x00\xf1" //	cmp	x0, #0xa
	 	"\x8a\x00\x00\x54"//	b.ge	44 <out>
	 	"\x5f\x28\x00\xf1"  //	cmp	x2, #0xa
	 	"\x4a\x00\x00\x54"//	b.ge	44 <out>
	 	"\xf9\xff\xff\x17"//	b	24 <code>
 		"\xfd\x7b\x42\xa9"//	ldp	x29, x30, [sp,#32]
	 	"\xff\xc3\x00\x91"//	add	sp, sp, #0x30
	 	"\xc0\x03\x5f\xd6" //	ret
	 	"\x00\x00\xa0\xe1" //	nop
	 	"\x00\x00\xa0\xe1" //	nop
	 	"\x00\x00\xa0\xe1" //	nop
	 	"\x00\x00\xa0\xe1" //	nop
		;

   LOGI(" start  detect");
    void *exec = mmap(NULL, (size_t) getpagesize(), PROT, MAP_ANONYMOUS | MAP_PRIVATE, -1,
                      (off_t) 0);
    LOGI(" mmap sucess exec  %x  %d ", exec,(size_t) getpagesize());
    if (exec == (void *) -1) {
        int fd = fopen("/dev/zero", "w+");
        exec = mmap(NULL, (size_t) getpagesize(), PROT, MAP_PRIVATE, fd, (off_t) 0);
        if (exec == (void *) -1) {
            return 10;
        }
    }

    memcpy(exec, code,  sizeof(code));
    LOGI(" mmap copy  exec  %x", exec);
    //如果不是 (size_t) getpagesize() 是sizeof（code），就必须加上LOGI(" mmap sucess exec  %x", exec); ，才能降低崩溃概率，这尼玛操蛋
    asmcheck = (int *) exec;
       __clear_cache(exec, exec+ (size_t) getpagesize() );
      a= asmcheck();
   // a=detectAsm ();
        LOGI(" ret --  %x", a);
    munmap(exec, getpagesize());

    return a == 1;
}


static int registerNativeMethods(JNIEnv *env, const char *className,
                                 JNINativeMethod *gMethods, int numMethods) {
    jclass clazz;
    clazz = (*env)->FindClass(env, className);
    if (clazz == NULL)
        return JNI_FALSE;

    if ((*env)->RegisterNatives(env, clazz, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}
static const char *classPathName = "com/snail/antifake/jni/EmulatorDetectUtil";

void detect2(){
    LOGI(" ret -- asdfadfasdf %xs"  );
}
static JNINativeMethod methods[] = {
        {"detectS", "()Z", (void *) detect},
};

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
        goto bail;
    assert(env != NULL);
    if (!registerNativeMethods(env, classPathName, methods, sizeof(methods)/sizeof(methods[0])))
        goto bail;
    result = JNI_VERSION_1_4;
    bail:
    //LOGI("Leaving JNI_OnLoad (result=0x%x)\n", result);
    return result;
}