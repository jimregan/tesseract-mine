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

#define LOG_TAG "OcrLib"
#include <utils/Log.h>

static tesseract::TessBaseAPI  api;
static jbyteArray image_obj;
static jbyte* image_buffer;

jboolean
ocr_open(JNIEnv *env, jobject thiz, jstring lang)
{
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
    if (api.Init("/sdcard/", c_lang)) {
        LOGE("could not initialize tesseract!");
        res = JNI_FALSE;
    }
#if DEBUG
    else if (!api.ReadConfigFile("/sdcard/tessdata/ratings")) {
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

#if DEBUG
static void dump_debug_data(char *text)
{
	if (tessedit_write_images) {
		page_image.write("/data/tessinput.tif");
	}

    if (text) {
        const char *outfile = "/data/out.txt";
        LOGI("write to output %s\n", outfile);
        FILE* fp = fopen(outfile, "w");
        if (fp != NULL) {
            fwrite(text, strlen(text), 1, fp);
            fclose(fp);
        }
    }
}
#else
static void dump_debug_data(char *text __attribute__((unused)))
{
}
#endif

jstring
ocr_recognize_image(JNIEnv *env, jobject thiz,
                    jbyteArray image,
                    jint width, jint height, 
                    jint bpp,
                    jint rowWidth)
{
	LOGI("recognize image x=%d, y=%d, rw=%d\n", width, height, rowWidth);

    if (env->GetArrayLength(image) < width * height) {
        LOGE("image length = %d is less than width * height = %d!",
             env->GetArrayLength(image),
             width * height);
    }

    jbyte* buffer = env->GetByteArrayElements(image, NULL);
	api.SetImage((const unsigned char *)buffer,
                 width, height, bpp, rowWidth);
	char * text = api.GetUTF8Text();
    env->ReleaseByteArrayElements(image, buffer, JNI_ABORT);

    dump_debug_data(text);

    // Will that work on a NULL?
    return env->NewStringUTF(text);
}

void
ocr_set_image(JNIEnv *env, jobject thiz,
              jbyteArray image,
              jint width, jint height, 
              jint bpp,
              jint rowWidth)
{
    LOG_ASSERT(image_obj == NULL && image_buffer == NULL,
               "image and/or image_buffer are not NULL!");
    image_obj = (jbyteArray)env->NewGlobalRef(image);
    image_buffer = env->GetByteArrayElements(image_obj, NULL);
	api.SetImage((const unsigned char *)image_buffer,
                 width, height, bpp, rowWidth);
}

void
ocr_set_rectangle(JNIEnv *env, jobject thiz,
                  jint left, jint top, 
                  jint width, jint height)
{
    // Restrict recognition to a sub-rectangle of the image. Call after SetImage.
    // Each SetRectangle clears the recogntion results so multiple rectangles
    // can be recognized with the same image.
    LOG_ASSERT(image_obj != NULL && image_buffer != NULL,
               "image and/or image_buffer are NULL!");
    api.SetRectangle(left, top, width, height);
}

jstring
ocr_recognize(JNIEnv *env, jobject thiz,
              jint width, jint height, 
              jint bpp,
              jint rowWidth)
{
    LOG_ASSERT(image_obj != NULL && image_buffer != NULL,
               "image and/or image_buffer are NULL!");

	char * text = api.GetUTF8Text();

    dump_debug_data(text);

    // Will that work on a NULL?
    return env->NewStringUTF(text);
}

static jint
ocr_mean_confidence(JNIEnv *env, jobject thiz)
{
    // Returns the (average) confidence value between 0 and 100.
    return api.MeanTextConf();
}

static jintArray
ocr_word_confidences(JNIEnv *env, jobject thiz)
{
    // Returns all word confidences (between 0 and 100) in an array, terminated
    // by -1.  The calling function must delete [] after use.
    // The number of confidences should correspond to the number of space-
    // delimited words in GetUTF8Text.
    int* confs = api.AllWordConfidences();
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
    // Set the value of an internal "variable" (of either old or new types).
    // Supply the name of the variable and the value as a string, just as
    // you would in a config file.
    // Returns false if the name lookup failed.
    // Eg SetVariable("tessedit_char_blacklist", "xyz"); to ignore x, y and z.
    // Or SetVariable("bln_numericmode", "1"); to set numeric-only mode.
    // SetVariable may be used before Init, but settings will revert to
    // defaults on End().
    
    const char *c_var  = env->GetStringUTFChars(var, NULL);
    const char *c_value  = env->GetStringUTFChars(value, NULL);

    api.SetVariable(c_var, c_value);

    env->ReleaseStringUTFChars(var, c_var);
    env->ReleaseStringUTFChars(value, c_value);
}

static void
ocr_clear_results(JNIEnv *env, jobject thiz)
{
    // Free up recognition results and any stored image data, without actually
    // freeing any recognition data that would be time-consuming to reload.
    // Afterwards, you must call SetImage or TesseractRect before doing
    // any Recognize or Get* operation.
    LOGI("releasing all memory");
    api.Clear();

    // Call between pages or documents etc to free up memory and forget
    // adaptive data.
    LOGI("clearing adaptive classifier");
    api.ClearAdaptiveClassifier();

    if (image_buffer != NULL) {
        LOGI("releasing image buffer");
        env->ReleaseByteArrayElements(image_obj, image_buffer, JNI_ABORT);
        env->DeleteGlobalRef(image_obj);
        image_obj = NULL;
        image_buffer = NULL;
    }
}

static void
ocr_close(JNIEnv *env, jobject thiz)
{
    LOGI("quit");
    // Close down tesseract and free up all memory. End() is equivalent to
    // destructing and reconstructing your TessBaseAPI.  Once End() has been
    // used, none of the other API functions may be used other than Init and
    // anything declared above it in the class definition.
    api.End();
}

static void
ocr_set_page_seg_mode(JNIEnv *env, jobject thiz, jint mode)
{
    // Set the current page segmentation mode. Defaults to PSM_AUTO.
    // api.SetPageSegMode((tesseract::PageSegMode)mode);
}

static JNINativeMethod methods[] = {
    {"openNative", "(Ljava/lang/String;)Z", (void*)ocr_open},
    {"setImageNative", "([BIIII)V", (void*)ocr_set_image},
    {"setRectangleNative", "(IIII)V", (void*)ocr_set_rectangle},
    {"recognizeNative", "()Ljava/lang/String;", (void*)ocr_recognize},
    {"recognizeNative", "([BIIII)Ljava/lang/String;", (void*)ocr_recognize_image},
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
