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
#include <stdio.h> // debug

#include "baseapi.h"
#include "varable.h"
#include "tessvars.h"

BOOL_VAR (tessedit_write_images, TRUE,
                 "Capture the image from the IPE");

#define LOG_TAG "OcrLib"
#include <utils/Log.h>

static tesseract::TessBaseAPI  api;

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
    else if (!api.ReadConfigFile("/sdcard/tessdata/ratings")) {
        LOGE("could not read config file, using defaults!");
        // This is not a fatal error.
    }

    env->ReleaseStringUTFChars(lang, c_lang);
    LOGI("successfully initialized tesseract!");
    return res;
}

jstring
ocr_recognize(JNIEnv *env, jobject thiz,
              jbyteArray image,
              jint width, jint height, jint rowWidth)
{
    int x = width, y = height, rw = rowWidth;
	LOGI("recognize image x=%d, y=%d, rw=%d\n", x, y, rw);

    if (env->GetArrayLength(image) < width * height) {
        LOGE("image length = %ld is less than width * height = %ld!",
             env->GetArrayLength(image),
             width * height);
    }

    jbyte* buffer = env->GetByteArrayElements(image, NULL);
	api.SetImage((const unsigned char *)buffer, x, y, 1, rw);
	char * text = api.GetUTF8Text();
    env->ReleaseByteArrayElements(image, buffer, JNI_ABORT);

	if (tessedit_write_images) {
		page_image.write("/data/tessinput.tif");
	}

    if (text) { // debug
        const char *outfile = "/data/out.txt";
        LOGI("write to output %s\n", outfile);
        FILE* fp = fopen(outfile, "w");
        if (fp != NULL) {
            fwrite(text, strlen(text), 1, fp);
            fclose(fp);
        }
    }

    // Will that work on a NULL?
    return env->NewStringUTF(text);
}

static void
ocr_close(JNIEnv *env, jobject thiz)
{
    LOGI("quit");
}

static JNINativeMethod methods[] = {
    {"open", "(Ljava/lang/String;)Z", (void*)ocr_open},
    {"recognize", "([BIII)Ljava/lang/String;", (void*)ocr_recognize},
    {"close", "()V", (void*)ocr_close},
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
