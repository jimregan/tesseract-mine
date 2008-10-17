/* 
**
** Copyright 2008, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <nativehelper/jni.h>
#include <assert.h>

#include "baseapi.h"
#include "varable.h"
#include "tessvars.h"

#define DEBUG 1

#if DEBUG
#include <stdio.h>
BOOL_VAR (tessedit_write_images, TRUE,
          "Capture the image from the IPE");
#endif

#define LOG_NDEBUG 0
#define LOG_TAG "OcrLib(native)"
#include <utils/Log.h>

static jfieldID field_mNativeData;

struct native_data_t {
    native_data_t() : image_obj(NULL), image_buffer(NULL) {}
    tesseract::TessBaseAPI api;
    jbyteArray image_obj;
    jbyte* image_buffer;
};

static inline native_data_t * get_native_data(JNIEnv *env, jobject object) {
    return (native_data_t *)(env->GetIntField(object, field_mNativeData));
}

#if DEBUG

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FAILIF(cond, msg...) do {                 \
        if (cond) { 	                          \
	        LOGE("%s(%d): ", __FILE__, __LINE__); \
            LOGE(msg);                            \
            return;                               \
        }                                         \
} while(0)

void test_ocr(const char *infile, int x, int y, int bpp,
              const char *outfile, const char *lang,
              const char *ratings, const char *tessdata)
{
	void *buffer;
	struct stat s;
	int ifd, ofd;

	LOGI("input file %s\n", infile);
	ifd = open(infile, O_RDONLY);
	FAILIF(ifd < 0, "open(%s): %s\n", infile, strerror(errno));
	FAILIF(fstat(ifd, &s) < 0, "fstat(%d): %s\n", ifd, strerror(errno));
	LOGI("file size %lld\n", s.st_size);
	buffer = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, ifd, 0);
	FAILIF(buffer == MAP_FAILED, "mmap(): %s\n", strerror(errno));
	LOGI("infile mmapped at %p\n", buffer);
	FAILIF(!tessdata, "You must specify a path for tessdata.\n");

	tesseract::TessBaseAPI  api;

	LOGI("tessdata %s\n", tessdata);
	LOGI("lang %s\n", lang);
	FAILIF(api.Init(tessdata, lang), "could not initialize tesseract\n");
	if (ratings) {
		LOGI("ratings %s\n", ratings);
		FAILIF(false == api.ReadConfigFile(ratings),
			"could not read config file\n");
	}

	LOGI("set image x=%d, y=%d bpp=%d\n", x, y, bpp);
	FAILIF(!bpp || bpp == 2 || bpp > 4, 
		"Invalid value %d of bpp\n", bpp);
	api.SetImage((const unsigned char *)buffer, x, y, bpp, bpp*x); 

	LOGI("set rectangle to cover entire image\n");
	api.SetRectangle(0, 0, x, y);

	LOGI("set page seg mode to single character\n");
	api.SetPageSegMode(tesseract::PSM_SINGLE_CHAR);
	LOGI("recognize\n");
	char * text = api.GetUTF8Text();
	if (tessedit_write_images) {
		page_image.write("tessinput.tif");
	}
	FAILIF(text == NULL, "didn't recognize\n");

	FILE* fp = fopen(outfile, "w");
	if (fp != NULL) {
        LOGI("write to output %s\n", outfile);
		fwrite(text, strlen(text), 1, fp);
		fclose(fp);
	}
    else LOGI("could not write to output %s\n", outfile);

	int mean_confidence = api.MeanTextConf();
	LOGI("mean confidence: %d\n", mean_confidence);

	int* confs = api.AllWordConfidences();
	int len, *trav;
	for (len = 0, trav = confs; *trav != -1; trav++, len++)
		LOGI("confidence %d: %d\n", len, *trav);
	free(confs);

	LOGI("clearing api\n");
	api.Clear();
	LOGI("clearing adaptive classifier\n");
	api.ClearAdaptiveClassifier();

	LOGI("clearing text\n");
	delete [] text;
}
#endif

jboolean
ocr_open(JNIEnv *env, jobject thiz, jstring lang)
{
    LOGV(__FUNCTION__);

    native_data_t *nat = get_native_data(env, thiz);

    if (lang == NULL) {
        LOGE("lang string is null!");
        return JNI_FALSE;
    }

    const char *c_lang = env->GetStringUTFChars(lang, NULL);
    if (c_lang == NULL) {
        LOGE("could not extract lang string!");
        return JNI_FALSE;
    }

    jboolean res = JNI_TRUE;

    LOGI("lang %s\n", c_lang);
    if (nat->api.Init("/sdcard/", c_lang)) {
        LOGE("could not initialize tesseract!");
        res = JNI_FALSE;
    }
#if DEBUG
    else if (!nat->api.ReadConfigFile("/sdcard/tessdata/ratings")) {
        LOGE("could not read config file, using defaults!");
        // This is not a fatal error.
    }
#endif
    else
        LOGI("lang %s initialization complete\n", c_lang);

    env->ReleaseStringUTFChars(lang, c_lang);
    LOGI("successfully initialized tesseract!");
    return res;
}

static void dump_debug_data(char *text)
{
#if DEBUG
	if (tessedit_write_images) {
		page_image.write("/sdcard/tessinput.tif");
	}

    if (text) {
        const char *outfile = "/sdcard/out.txt";
        LOGI("write to output %s\n", outfile);
        FILE* fp = fopen(outfile, "w");
        if (fp != NULL) {
            fwrite(text, strlen(text), 1, fp);
            fclose(fp);
        }
    }
#endif
}

jstring
ocr_recognize_image(JNIEnv *env, jobject thiz,
                    jbyteArray image,
                    jint width, jint height, 
                    jint bpp)
{
    LOGV(__FUNCTION__);

	LOGI("recognize image x=%d, y=%d bpp=%d\n", width, height, bpp);

    native_data_t *nat = get_native_data(env, thiz);

    if (env->GetArrayLength(image) < width * height) {
        LOGE("image length = %d is less than width * height = %d!",
             env->GetArrayLength(image),
             width * height);
    }

    jbyte* buffer = env->GetByteArrayElements(image, NULL);
	nat->api.SetImage((const unsigned char *)buffer,
                 width, height, bpp, bpp*width);
	char * text = nat->api.GetUTF8Text();
    env->ReleaseByteArrayElements(image, buffer, JNI_ABORT);

    dump_debug_data(text);

    // Will that work on a NULL?
    return env->NewStringUTF(text);
}

void
ocr_set_image(JNIEnv *env, jobject thiz,
              jbyteArray image,
              jint width, jint height, 
              jint bpp)
{
    LOGV(__FUNCTION__);

	LOGI("set image x=%d, y=%d, bpp=%d\n", width, height, bpp);

    native_data_t *nat = get_native_data(env, thiz);

    LOG_ASSERT(nat->image_obj == NULL && nat->image_buffer == NULL,
               "image %p and/or image_buffer %p are not NULL!",
               nat->image_obj,
               nat->image_buffer);

    nat->image_obj = (jbyteArray)env->NewGlobalRef(image);
    nat->image_buffer = env->GetByteArrayElements(nat->image_obj, NULL);
    LOG_ASSERT(nat->image_buffer != NULL, "image buffer is NULL!");
	nat->api.SetImage((const unsigned char *)nat->image_buffer,
                      width, height, bpp, bpp*width);
}

void
ocr_set_rectangle(JNIEnv *env, jobject thiz,
                  jint left, jint top, 
                  jint width, jint height)
{
    LOGV(__FUNCTION__);
    // Restrict recognition to a sub-rectangle of the image. Call after SetImage.
    // Each SetRectangle clears the recogntion results so multiple rectangles
    // can be recognized with the same image.
    native_data_t *nat = get_native_data(env, thiz);

	LOGI("set rectangle left=%d, top=%d, width=%d, height=%d\n",
         left, top, width, height);

    LOG_ASSERT(nat->image_obj != NULL && nat->image_buffer != NULL,
               "image and/or image_buffer are NULL!");
    nat->api.SetRectangle(left, top, width, height);
}

jstring
ocr_recognize(JNIEnv *env, jobject thiz)
{
    LOGV(__FUNCTION__);

    native_data_t *nat = get_native_data(env, thiz);

    LOG_ASSERT(nat->image_obj != NULL && nat->image_buffer != NULL,
               "image and/or image_buffer are NULL!");

    LOGI("BEFORE RECOGNIZE");
	char * text = nat->api.GetUTF8Text();
    LOGI("AFTER RECOGNIZE");

    dump_debug_data(text);

    // Will that work on a NULL?
    return env->NewStringUTF(text);
}

static jint
ocr_mean_confidence(JNIEnv *env, jobject thiz)
{
    LOGV(__FUNCTION__);
    native_data_t *nat = get_native_data(env, thiz);
    // Returns the (average) confidence value between 0 and 100.
    return nat->api.MeanTextConf();
}

static jintArray
ocr_word_confidences(JNIEnv *env, jobject thiz)
{
    LOGV(__FUNCTION__);
    // Returns all word confidences (between 0 and 100) in an array, terminated
    // by -1.  The calling function must delete [] after use.
    // The number of confidences should correspond to the number of space-
    // delimited words in GetUTF8Text.
    native_data_t *nat = get_native_data(env, thiz);
    int* confs = nat->api.AllWordConfidences();
    LOG_ASSERT(confs != NULL, "Could not get word-confidence values!");

    int len, *trav;
    for (len = 0, trav = confs; *trav != -1; trav++, len++);

    LOG_ASSERT(confs != NULL, "Confidence array has %d elements",
               len);

    jintArray ret = env->NewIntArray(len);
    LOG_ASSERT(ret != NULL,
               "Could not create Java confidence array!");

    env->SetIntArrayRegion(ret, 0, len, confs);    
    delete [] confs;
    return ret;
}

static void
ocr_set_variable(JNIEnv *env, jobject thiz,
                 jstring var, jstring value)
{
    LOGV(__FUNCTION__);
    // Set the value of an internal "variable" (of either old or new types).
    // Supply the name of the variable and the value as a string, just as
    // you would in a config file.
    // Returns false if the name lookup failed.
    // Eg SetVariable("tessedit_char_blacklist", "xyz"); to ignore x, y and z.
    // Or SetVariable("bln_numericmode", "1"); to set numeric-only mode.
    // SetVariable may be used before Init, but settings will revert to
    // defaults on End().

    native_data_t *nat = get_native_data(env, thiz);
    
    const char *c_var  = env->GetStringUTFChars(var, NULL);
    const char *c_value  = env->GetStringUTFChars(value, NULL);

    nat->api.SetVariable(c_var, c_value);

    env->ReleaseStringUTFChars(var, c_var);
    env->ReleaseStringUTFChars(value, c_value);
}

static void
ocr_clear_results(JNIEnv *env, jobject thiz)
{
    LOGV(__FUNCTION__);
    // Free up recognition results and any stored image data, without actually
    // freeing any recognition data that would be time-consuming to reload.
    // Afterwards, you must call SetImage or TesseractRect before doing
    // any Recognize or Get* operation.
    LOGI("releasing all memory");
    native_data_t *nat = get_native_data(env, thiz);
    nat->api.Clear();

    // Call between pages or documents etc to free up memory and forget
    // adaptive data.
    LOGI("clearing adaptive classifier");
    nat->api.ClearAdaptiveClassifier();

    if (nat->image_buffer != NULL) {
        LOGI("releasing image buffer");
        env->ReleaseByteArrayElements(nat->image_obj,
                                      nat->image_buffer, JNI_ABORT);
        env->DeleteGlobalRef(nat->image_obj);
        nat->image_obj = NULL;
        nat->image_buffer = NULL;
    }
}

static void
ocr_close(JNIEnv *env, jobject thiz)
{
    LOGV(__FUNCTION__);
    // Close down tesseract and free up all memory. End() is equivalent to
    // destructing and reconstructing your TessBaseAPI.  Once End() has been
    // used, none of the other API functions may be used other than Init and
    // anything declared above it in the class definition.
    native_data_t *nat = get_native_data(env, thiz);
    nat->api.End();
}

static void
ocr_set_page_seg_mode(JNIEnv *env, jobject thiz, jint mode)
{
    LOGV(__FUNCTION__);
    native_data_t *nat = get_native_data(env, thiz);
    nat->api.SetPageSegMode((tesseract::PageSegMode)mode);
}

static void class_init(JNIEnv* env, jclass clazz) {
    LOGV(__FUNCTION__);
    field_mNativeData = env->GetFieldID(clazz, "mNativeData", "I");
#if DEBUG && 0
    test_ocr("/sdcard/chi.yuv", 106, 106, 1,
             "/sdcard/out.txt", "chi_sim0",
             "/sdcard/tessdata/ratings", "/sdcard/");
#endif
}

static void initialize_native_data(JNIEnv* env, jobject object) {
    LOGV(__FUNCTION__);
    native_data_t *nat = new native_data_t;
    if (nat == NULL) {
        LOGE("%s: out of memory!", __FUNCTION__);
        return;
    }

    env->SetIntField(object, field_mNativeData, (jint)nat);
}

static void cleanup_native_data(JNIEnv* env, jobject object) {
    LOGV(__FUNCTION__);
    native_data_t *nat = get_native_data(env, object);
    if (nat)
        delete nat;
}

static JNINativeMethod methods[] = {
     /* name, signature, funcPtr */
    {"classInitNative", "()V", (void*)class_init},
    {"initializeNativeDataNative", "()V", (void *)initialize_native_data},
    {"cleanupNativeDataNative", "()V", (void *)cleanup_native_data},

    {"openNative", "(Ljava/lang/String;)Z", (void*)ocr_open},
    {"setImageNative", "([BIII)V", (void*)ocr_set_image},
    {"setRectangleNative", "(IIII)V", (void*)ocr_set_rectangle},
    {"recognizeNative", "()Ljava/lang/String;", (void*)ocr_recognize},
    {"recognizeNative", "([BIII)Ljava/lang/String;", (void*)ocr_recognize_image},
    {"clearResultsNative", "()V", (void*)ocr_clear_results},
    {"closeNative", "()V", (void*)ocr_close},
    {"meanConfidenceNative", "()I", (void*)ocr_mean_confidence},
    {"wordConfidencesNative", "()[I", (void*)ocr_word_confidences},
    {"setVariableNative", "(Ljava/lang/String;Ljava/lang/String;)V", (void*)ocr_set_variable},
    {"setPageSegModeNative", "(I)V", (void*)ocr_set_page_seg_mode},
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz = env->FindClass(className);

    if (clazz == NULL) {
        LOGE("Native registration unable to find class %s", className);
        return JNI_FALSE;
    }

    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        LOGE("RegisterNatives failed for %s", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */

typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    JNIEnv* env = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("GetEnv failed\n");
        return (jint)-1;
    }
    env = uenv.env;

    assert(env != NULL);

    LOGI("In OcrLib JNI_OnLoad\n");

    if (JNI_FALSE ==
        registerNativeMethods(env, 
                              "com/android/ocr/OcrLib",
                              methods,
                              sizeof(methods) / sizeof(methods[0]))) {
        LOGE("OcrLib native registration failed\n");
        return (jint)-1;
    }

    /* success -- return valid version number */
    LOGI("OcrLib native registration succeeded!\n");
    return (jint)JNI_VERSION_1_4;
}
