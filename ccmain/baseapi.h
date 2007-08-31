///////////////////////////////////////////////////////////////////////
// File:        baseapi.h
// Description: Simple API for calling tesseract.
// Author:      Ray Smith
// Created:     Fri Oct 06 15:35:01 PDT 2006
//
// (C) Copyright 2006, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////

#ifndef THIRD_PARTY_TESSERACT_CCMAIN_BASEAPI_H__
#define THIRD_PARTY_TESSERACT_CCMAIN_BASEAPI_H__

class PAGE_RES;
class BLOCK_LIST;

// Base class for all tesseract APIs.
// Specific classes can add ability to work on different inputs or produce
// different outputs.

class TessBaseAPI {
 public:
  // Start tesseract.
  // The datapath must be the name of the data directory or some other file
  // in which the data directory resides (for instance argv[0].)
  // The configfile is the name of a file in the tessconfigs directory
  // (eg batch) or NULL to run on defaults.
  // Outputbase may also be NULL, and is the basename of various output files.
  // If the output of any of these files is enabled, then a name must be given.
  // If numeric_mode is true, only possible digits and roman numbers are
  // returned. Returns 0 if successful. Crashes if not.
  // The argc and argv may be 0 and NULL respectively. They are used for
  // providing config files for debug/display purposes.
  // TODO(rays) get the facts straight. Is it OK to call
  // it more than once? Make it properly check for errors and return them.
  static int Init(const char* datapath, const char* outputbase,
                  const char* configfile, bool numeric_mode,
                  int argc, char* argv[]);

  // Start tesseract.
  // Similar to Init() except that it is possible to specify the language.
  // Language is the code of the language for which the data will be loaded.
  // (Codes follow ISO 639-2.) If it is NULL, english (eng) will be loaded.
  static int InitWithLanguage(const char* datapath, const char* outputbase,
                              const char* language, const char* configfile,
                              bool numeric_mode, int argc, char* argv[]);

  // Set the name of the input file. Needed only for training and
  // reading a UNLV zone file.
  static void SetInputName(const char* name);

  // Recognize a rectangle from an image and return the result as a string.
  // May be called many times for a single Init.
  // Currently has no error checking.
  // Greyscale of 8 and color of 24 or 32 bits per pixel may be given.
  // Palette color images will not work properly and must be converted to
  // 24 bit.
  // Binary images of 1 bit per pixel may also be given but they must be
  // byte packed with the MSB of the first byte being the first pixel, and a
  // 1 represents WHITE. For binary images set bytes_per_pixel=0.
  // The recognized text is returned as a char* which (in future will be coded
  // as UTF8 and) must be freed with the delete [] operator.
  static char* TesseractRect(const unsigned char* imagedata,
                             int bytes_per_pixel,
                             int bytes_per_line,
                             int left, int top, int width, int height);
  // As TesseractRect but produces a box file as output.
  // Image height is needed as well as rect height, since output y-coords
  // will be relative to the bottom of the image.
  static char* TesseractRectBoxes(const unsigned char* imagedata,
                                  int bytes_per_pixel,
                                  int bytes_per_line,
                                  int left, int top, int width, int height,
                                  int imageheight);
  // As TesseractRect but produces UNLV-style output.
  static char* TesseractRectUNLV(const unsigned char* imagedata,
                                 int bytes_per_pixel,
                                 int bytes_per_line,
                                 int left, int top, int width, int height);

  // Call between pages or documents etc to free up memory and forget
  // adaptive data.
  static void ClearAdaptiveClassifier();

  // Close down tesseract and free up memory.
  static void End();

  // Dump the internal binary image to a PGM file.
  static void DumpPGM(const char* filename);

 protected:
  // Copy the given image rectangle to Tesseract, with adaptive thresholding
  // if the image is not already binary.
  static void CopyImageToTesseract(const unsigned char* imagedata,
                                   int bytes_per_pixel,
                                   int bytes_per_line,
                                   int left, int top, int width, int height);

  // Compute the Otsu threshold(s) for the given image rectangle, making one
  // for each channel. Each channel is always one byte per pixel.
  // Returns an array of threshold values and an array of hi_values, such
  // that a pixel value >threshold[channel] is considered foreground if
  // hi_values[channel] is 0 or background if 1. A hi_value of -1 indicates
  // that there is no apparent foreground. At least one hi_value will not be -1.
  // thresholds and hi_values are assumed to be of bytes_per_pixel size.
  static void OtsuThreshold(const unsigned char* imagedata,
                           int bytes_per_pixel,
                           int bytes_per_line,
                           int left, int top, int right, int bottom,
                           int* thresholds,
                           int* hi_values);

  // Compute the histogram for the given image rectangle, and the given
  // channel. (Channel pointed to by imagedata.) Each channel is always
  // one byte per pixel.
  // Bytes per pixel is used to skip channels not being
  // counted with this call in a multi-channel (pixel-major) image.
  // Histogram is always a 256 element array to count occurrences of
  // each pixel value.
  static void HistogramRect(const unsigned char* imagedata,
                            int bytes_per_pixel,
                            int bytes_per_line,
                            int left, int top, int right, int bottom,
                            int* histogram);

  // Compute the Otsu threshold(s) for the given histogram.
  // Also returns H = total count in histogram, and
  // omega0 = count of histogram below threshold.
  static int OtsuStats(const int* histogram,
                       int* H_out,
                       int* omega0_out);

  // Threshold the given grey or color image into the tesseract global
  // image ready for recognition. Requires thresholds and hi_value
  // produced by OtsuThreshold above.
  static void ThresholdRect(const unsigned char* imagedata,
                            int bytes_per_pixel,
                            int bytes_per_line,
                            int left, int top,
                            int width, int height,
                            const int* thresholds,
                            const int* hi_values);

  // Cut out the requested rectangle of the binary image to the
  // tesseract global image ready for recognition.
  static void CopyBinaryRect(const unsigned char* imagedata,
                             int bytes_per_line,
                             int left, int top,
                             int width, int height);

  // Low-level function to recognize the current global image to a string.
  static char* RecognizeToString();

  // Find lines from the image making the BLOCK_LIST.
  static void FindLines(BLOCK_LIST* block_list);

  // Recognize the tesseract global image and return the result as Tesseract
  // internal structures.
  static PAGE_RES* Recognize(BLOCK_LIST* block_list,
                             struct ETEXT_STRUCT* monitor);

  // Return the maximum length that the output text string might occupy.
  static int TextLength(PAGE_RES* page_res);
  // Convert (and free) the internal data structures into a text string.
  static char* TesseractToText(PAGE_RES* page_res);
  // Make a text string from the internal data structures.
  // The input page_res is deleted.
  // The text string takes the form of a box file as needed for training.
  static char* TesseractToBoxText(PAGE_RES* page_res, int left, int bottom);
  // Make a text string from the internal data structures.
  // The input page_res is deleted. The text string is converted
  // to UNLV-format: Latin-1 with specific reject and suspect codes.
  static char* TesseractToUNLV(PAGE_RES* page_res);

  // __________________________   ocropus add-ons   ___________________________

  // Find lines from the image making the BLOCK_LIST.
  static BLOCK_LIST* FindLinesCreateBlockList();

  // Delete a block list.
  // This is to keep BLOCK_LIST pointer opaque
  // and let go of including the other headers.
  static void DeleteBlockList(BLOCK_LIST *);

  // Adapt to recognize the current image as the given character.
  // The image must be preloaded and be just an image of a single character.
  static void AdaptToCharacter(const char *unichar_repr,
                               int length,
                               float baseline,
                               float xheight,
                               float descender,
                               float ascender);

  // Recognize text doing one pass only, using settings for a given pass.
  static PAGE_RES* RecognitionPass1(BLOCK_LIST* block_list);
  static PAGE_RES* RecognitionPass2(BLOCK_LIST* block_list, PAGE_RES* pass1_result);

  // Extract the OCR results, costs (penalty points for uncertainty),
  // and the bounding boxes of the characters.
  static int TesseractExtractResult(char** string,
                                    int** lengths,
                                    float** costs,
                                    int** x0,
                                    int** y0,
                                    int** x1,
                                    int** y1,
                                    PAGE_RES* page_res);

  // If you call this before recognizing a page, Tesseract will use the color
  // page segmentation data to guide its segmentation process.  If you call
  // it with a null pointer, Tesseract will revert to internal layout analysis
  // (i.e., single column line finding).  The segdata pointer is shared with
  // Tesseract until SetPageSegmentation is called with a new pointer (i.e.,
  // Tesseract doesn't make its own internal copy).  This image must have the
  // same width and height as any image being recognized while this segmentation
  // is in effect.
  // The segdata values are RGB triples, with distinct R values denoting distinct
  // "blocks" or "columns" and distinct GB pairs denoting distinct lines.
  // Lexicographic ordering of the RGB triples corresponds to text output order, with
  // linebreaks inserted between distinct GB values and double blank lines between 
  // distinct R values.
  static int SetPageSegmentation(const unsigned char *segdata,
                                 int bytes_per_pixel, /* must be 3 */
                                 int top, int left, int width, int height);

  // New calling interface into Tesseract that permits baseline data to be passed.
  // The input to this should be a single line of text.
  static int TesseractWithBaseline(const unsigned char* imagedata,
                                     int bytes_per_pixel,
                                     int width, int height,
                                     int *baseline_ypos,
                                     int *xheight_ypos,
                                     char** string,
                                     int** lengths,
                                     float** costs,
                                     int** x0,
                                     int** y0,
                                     int** x1,
                                     int** y1,
                                     PAGE_RES* page_res);
};

#endif  // THIRD_PARTY_TESSERACT_CCMAIN_BASEAPI_H__
