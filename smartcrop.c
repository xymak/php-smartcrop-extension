/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_smartcrop.h"
#include "ext/gd/php_gd.h"
#include <math.h>
#define OPTION_DETAIL_WEIGHT 0.2
#define OPTION_SKIN_COLOR_R 0.78
#define OPTION_SKIN_COLOR_G 0.57
#define OPTION_SKIN_COLOR_B 0.44
#define OPTION_SKIN_BIAS 0.01
#define OPTION_SKIN_BRIGHTNESS_MIN 0.2
#define OPTION_SKIN_BRIGHTNESS_MAX 1.0
#define OPTION_SKIN_THRESHOLD 0.8
#define OPTION_SKIN_WEIGHT 1.8
#define OPTION_SATURATION_BRIGHTNESS_MIN 0.05
#define OPTION_SATURATION_BRIGHTNESS_MAX 0.9
#define OPTION_SATURATION_THRESHOLD 0.4
#define OPTION_SATURATION_BIAS 0.2
#define OPTION_SATURATION_WEIGHT 0.3
#define OPTION_SCORE_DOWN_SAMPLE 8
#define OPTION_STEP 8
#define OPTION_SCALE_STEP 0.1
#define OPTION_MIN_SCALE 1.0
#define OPTION_MAX_SCALE 1.0
#define OPTION_EDGE_RADIUS 0.4
#define OPTION_EDGE_WEIGHT -20.0
#define OPTION_OUTSIDE_IMPORTANCE -0.5
#define OPTION_BOOST_WEIGHT 100
#define OPTION_RULE_OF_THIRDS 1

/* If you declare any globals in php_smartcrop.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(smartcrop)
*/

/* True global resources - no need for thread safety here */
static int le_smartcrop;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("smartcrop.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_smartcrop_globals, smartcrop_globals)
    STD_PHP_INI_ENTRY("smartcrop.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_smartcrop_globals, smartcrop_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_smartcrop_compiled(string arg)
   Return a string to confirm that the module is compiled in */
static int *getRgbColorAt(zval *IM, zend_long X, zend_long Y){
    zval *function_name, *zv_r, *zv_g, *zv_b, *zv_a, *retval_ptr;
    zval get_colorindex_params[3];
    zval get_color_params[2];
    static int rgb[4],rgbHex;

    function_name = emalloc(sizeof(zval));
    retval_ptr = emalloc(sizeof(zval));

    ZVAL_STRING(function_name, "imagecolorat");
    ZVAL_ZVAL(&get_colorindex_params[0], IM, 1, 0);
    ZVAL_LONG(&get_colorindex_params[1], X);
    ZVAL_LONG(&get_colorindex_params[2], Y);
    call_user_function(EG(function_table),NULL,function_name, retval_ptr,3,get_colorindex_params TSRMLS_CC);
    rgbHex = Z_LVAL_P(retval_ptr);
    rgb[0] = rgbHex >> 16;
    rgb[1] = rgbHex >> 8 & 255;
    rgb[2] = rgbHex & 255;

    return rgb;
}
static float cie(float r, float g, float b) {
    return 0.5126 * b + 0.7152 * g + 0.0722 * r;
}
static float sample(zval *IM, zend_long X, zend_long Y){
    int *rgb_ptr;
    rgb_ptr = getRgbColorAt(IM,X,Y);
    return cie(*rgb_ptr,*(rgb_ptr+1),*(rgb_ptr+2));
}
static int edgeDetect(zval *IM, zend_long X, zend_long Y, zend_long W, zend_long H){
    float lightness, leftLightness, centerLightness, rightLightness, topLightness,bottomLightness;
    if (X == 0 || X >= W-1 || Y == 0 || Y >= H-1) {
        lightness = sample(IM,X,Y);
    } else {
        leftLightness = sample(IM,X-1,Y);
        centerLightness = sample(IM,X,Y);
        rightLightness = sample(IM,X+1,Y);
        topLightness = sample(IM,X,Y-1);
        bottomLightness = sample(IM,X,Y+1);
        lightness = centerLightness*4.0 - leftLightness - rightLightness - topLightness - bottomLightness;
    }
    return lightness;
}
static float skinColor(float r, float g, float b) {
    float mag, rd, gd, bd, d;
    mag = sqrt(r*r + g*g + b*b);
    rd = (r/mag - OPTION_SKIN_COLOR_R);
    gd = (g/mag - OPTION_SKIN_COLOR_G);
    bd = (b/mag - OPTION_SKIN_COLOR_B);
    d = sqrt(rd*rd + gd*gd+bd*bd);
    return 1-d;
}
static float skinDetect(float r, float g, float b, float lightness) {
    float skin;
    int isSkinColor, isSkinBrightness;    

    lightness = lightness / 255;
    skin = skinColor(r, g, b);
    isSkinColor = skin > OPTION_SKIN_THRESHOLD ? 1 : 0;
    isSkinBrightness = lightness > OPTION_SKIN_BRIGHTNESS_MIN && lightness <= OPTION_SKIN_BRIGHTNESS_MAX ? 1 : 0;
    
    if (isSkinColor && isSkinBrightness) {
        return (skin - OPTION_SKIN_THRESHOLD) * (255 / (1 - OPTION_SKIN_THRESHOLD));
    } else {
        return 0;
    }
}
static float saturation(float r, float g, float b){
    float maximum, minimum, l, d;
    
    maximum = (r/255) > (g/255) ? (r/255) : (g/255);
    maximum = maximum > (b/255) ? maximum : (b/255);
    minimum = (r/255) < (g/255) ? (r/255) : (g/255);
    minimum = minimum < (b/255) ? minimum : (b/255);

    if (maximum == minimum) {
        return 0;
    }
    
    l = (maximum + minimum) / 2;
    d = (maximum - minimum);

    return l > 0.5 ? d / (2 - maximum - minimum) : (d / (maximum + minimum));
}
static float saturationDetect(float r, float g, float b, float lightness){ 
    float sat;
    int acceptableSaturation, acceptableLightness;
    
    lightness = lightness / 255;
    sat = saturation(r, g, b);
    acceptableSaturation = sat > OPTION_SATURATION_THRESHOLD ? 1 : 0;
    acceptableLightness = (lightness >= OPTION_SATURATION_BRIGHTNESS_MIN && lightness <= OPTION_SATURATION_BRIGHTNESS_MAX) ? 1 : 0;
    if (acceptableLightness && acceptableSaturation) {
        return (sat - OPTION_SATURATION_THRESHOLD) * (255 / (1-OPTION_SATURATION_THRESHOLD));
    } else {
        return 0;
    }
}
static int *downSample(int w, int h, float *od){
    int p;
    float width, height, ifactor2, x, y, u, v, r, g, b, a, mr, mg, mb, pR, pG, pB, pA;
    static float *data;

    width = floor(w/OPTION_SCORE_DOWN_SAMPLE);
    height = floor(h/OPTION_SCORE_DOWN_SAMPLE);
    ifactor2 = 1.0 / (OPTION_SCORE_DOWN_SAMPLE * OPTION_SCORE_DOWN_SAMPLE);

    data = (float*)emalloc(width*height*4*sizeof(float));

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            r = g = b = a = mr = mg = mb = 0.0;

            for(v = 0; v < OPTION_SCORE_DOWN_SAMPLE; v++) {
                for (u = 0; u < OPTION_SCORE_DOWN_SAMPLE; u++) {
                    p = (y * OPTION_SCORE_DOWN_SAMPLE + v) * w * 3 + (x * OPTION_SCORE_DOWN_SAMPLE + u) * 3;
                    pR = *(od + p);
                    pG = *(od + p + 1);
                    pB = *(od + p + 2);
                    pA = 0.0;

                    r += pR;
                    g += pG;
                    b += pB;
                    a += pA;

                    mr = mr > pR ? mr : pR;
                    mg = mg > pG ? mg : pG;
                    mb = mb > pB ? mb : pB;
                }
            }

            p = y * width * 4 + x * 4;
            data[p] = r * ifactor2 * 0.5 + mr * 0.5;
            data[p+1] = g * ifactor2 * 0.7 + mg * 0.3;
            data[p+2] = b * ifactor2;
            data[p+3] = a * ifactor2;
        }
    }
    return data;
}
static int * generateCrops(int w, int h, int cw, int ch){
    int cropWidth, cropHeight, x, y, p;
    static int * result;
    cropWidth = cw;
    cropHeight = ch;
    if (cw == w) {
        result = (int*)emalloc(sizeof(int)*(h-cropHeight)/OPTION_STEP*2);
    } else {
        result = (int*)emalloc(sizeof(int)*(w-cropWidth)/OPTION_STEP*2);
    }
    p = 0;
    for (y = 0; y + cropHeight <= h; y += OPTION_STEP) {
        for (x = 0; x + cropWidth <= w; x += OPTION_STEP) {
            result[p]=x;
            result[p+1]=y;
            p += 2;                      
        }
    }
    return result;
}
static float thirds(float x) {
    x = ((float)((int)(x - (1.0/3.0) + 1.0) % 2) * 0.5 - 0.5) * 16.0;
    return (1.0 - x * x) > 0.0 ? (1.0 - x * x) : 0.0;
}
static float importance(float cx, float cy, float cw, float ch, float x, float y) {
    float px, py, dx, dy, d, s;
 
    if (cx > x || x >= cx + cw || cy > y || y > cy + ch) {
        return OPTION_OUTSIDE_IMPORTANCE;
    }
    x = (x - cx) / cw;
    y = (y - cy) / ch;
    px = fabs(0.5 - x) * 2.0;
    py = fabs(0.5 - y) * 2,0;
    dx = (px - 1.0 + OPTION_EDGE_RADIUS);
    dx = dx > 0.0 ? dx : 0.0;
    dy = (py - 1.0 + OPTION_EDGE_RADIUS);
    dy = dy > 0.0 ? dy : 0.0;
    d = (dx * dx + dy * dy) * OPTION_EDGE_WEIGHT;
    s = 1.41 - sqrt(px * px + py * py);
    if (OPTION_RULE_OF_THIRDS) {
        s += (0.0 > (s + d + 0.5) ? 0.0 : (s + d + 0.5)) * 1.2 * (thirds(px) + thirds(py));
    }
    return s+d;
}
static float score(float *output, int cx, int cy, int cw, int ch, int w, int h) {
    float downSample, invDownSample,i,detail,skin,saturation, total;
    int outputHeightDownSample, outputWidthDownSample, outputWidth, y, x, p;

    downSample = OPTION_SCORE_DOWN_SAMPLE;
    invDownSample = 1 /downSample;
    outputHeightDownSample = h;
    outputWidthDownSample = w;    
    outputWidth = w / downSample;
    
    detail = skin = saturation = 0;
    for (y = 0; y < outputHeightDownSample; y += downSample) {
        for (x = 0; x < outputWidthDownSample; x += downSample) {
           i = importance(cx, cy, cw, ch, x, y);
           p = floor(y / downSample) * outputWidth * 4 + floor(x / downSample) * 4;
           detail += (*(output+p+1) / 255) * i;
           //detail = p;
           skin += *(output + p) / 255 * (detail + OPTION_SKIN_BIAS) * i;
           saturation += *(output + p + 2) / 255 * (detail + OPTION_SATURATION_BIAS) * i;
        }
    }
    total = (detail * OPTION_DETAIL_WEIGHT + skin * OPTION_SKIN_WEIGHT + saturation * OPTION_SATURATION_WEIGHT) / (cw * ch);
    return total;
    
}
PHP_FUNCTION(smartcrop)
{
    zval *SIM;
    zend_long DW, DH;
    zval *function_name;
    zval *retval_ptr;
    zend_long SW, SH, RW, RH;
    zval *IM_CANVAS;
    zval *RIM;
    float sw, sh, dw, dh, rw, rh, scale;
    
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zll", &SIM, &DW, &DH )
        == FAILURE)
    {
        RETURN_FALSE;
    }

    function_name = emalloc(sizeof(zval));
    retval_ptr = emalloc(sizeof(zval));
    IM_CANVAS = emalloc(sizeof(zval));
    RIM = emalloc(sizeof(zval));
    SW = emalloc(sizeof(zval));
    SH = emalloc(sizeof(zval));

    //zval **params=(zval**)emalloc(sizeof(zval));
    zval **params;
    params = SIM;

    /* Get image source size*/
    ZVAL_STRING(function_name, "imagesx");
    call_user_function(EG(function_table),NULL,function_name,retval_ptr,1,params TSRMLS_CC);
    SW = Z_LVAL_P(retval_ptr);
    
    ZVAL_STRING(function_name, "imagesy");
    call_user_function(EG(function_table),NULL,function_name,retval_ptr,1,params TSRMLS_CC);
    SH = Z_LVAL_P(retval_ptr);

    sw = SW;
    sh = SH;
    dw = DW;
    dh = DH;
    scale = (dw / sw) > (dh / sh) ? (dw / sw) : (dh / sh);
    RW = floor(sw * scale);
    RH = floor(sh * scale);

    zval canvas_params[2];
    ZVAL_LONG(&canvas_params[0], RW);
    ZVAL_LONG(&canvas_params[1], RH);
    
    /* Scale image size */
    ZVAL_STRING(function_name, "imagecreatetruecolor");
    call_user_function(EG(function_table),NULL,function_name,IM_CANVAS,2,canvas_params TSRMLS_CC);
    
    zval resize_params[10];
    ZVAL_ZVAL(&resize_params[0], IM_CANVAS, 1, 0);
    ZVAL_ZVAL(&resize_params[1], SIM, 1, 0);
    ZVAL_LONG(&resize_params[2],0);
    ZVAL_LONG(&resize_params[3],0);
    ZVAL_LONG(&resize_params[4],0);
    ZVAL_LONG(&resize_params[5],0);
    ZVAL_LONG(&resize_params[6],RW);
    ZVAL_LONG(&resize_params[7],RH);
    ZVAL_LONG(&resize_params[8],SW);
    ZVAL_LONG(&resize_params[9],SH);
    ZVAL_STRING(function_name, "imagecopyresampled");
    call_user_function(EG(function_table),NULL,function_name,RIM,10,resize_params TSRMLS_CC);

    //RETURN_ZVAL(IM_CANVAS, 1, 1);

    int x,y,p,z;
    rw = RW;
    rh = RH;
    int *rgb_ptr;
    float *od;
    float *scoreOutput;
    int *crops;

    od = (float*)emalloc(RW*RH*3*sizeof(float));
    
    for (y = 0; y < rh; y++) {
        for (x = 0; x < rw; x++) {
            rgb_ptr = getRgbColorAt(IM_CANVAS,x,y);
            p = y * rw * 3 + x * 3;
            od[p+1] = edgeDetect(IM_CANVAS,x,y,rw,rh);
            od[p] = skinDetect(*(rgb_ptr),*(rgb_ptr+1),*(rgb_ptr+2),sample(IM_CANVAS,x,y));
            od[p+2] = saturationDetect(*(rgb_ptr),*(rgb_ptr+1),*(rgb_ptr+2),sample(IM_CANVAS,x,y)); 
        }
    }
    
    scoreOutput = downSample(rw, rh, od);
    //RETURN_DOUBLE(*(scoreOutput+2)); return;
    crops = generateCrops(rw, rh, dw, dh);
    float topScore = -1.0/0.0;
    int topCrop;
    float scoreTmp;
    int cropsNum;
    
    if (dw == rw) {
        cropsNum = (rh - dh)/OPTION_STEP;
    } else {
        cropsNum = (rw - dw)/OPTION_STEP;
    }
    
//    array_init(return_value);
    for (z = 0; z <= cropsNum*2; z += 2){
        scoreTmp = score(scoreOutput, *(crops+z), *(crops+z+1),dw,dh,rw,rh);
//        add_next_index_double(return_value, scoreTmp);
        if (topScore < scoreTmp){
            topScore = scoreTmp;
            topCrop = z;
        }
    }
    
//    array_init(return_value);
//    add_next_index_long(return_value, *(crops+topCrop)); 
//    add_next_index_long(return_value, *(crops+topCrop+1));

    zval *CIM;
    ZVAL_LONG(&canvas_params[0], DW);
    ZVAL_LONG(&canvas_params[1], DH);
    
    CIM = emalloc(sizeof(zval));

    ZVAL_STRING(function_name, "imagecreatetruecolor");
    call_user_function(EG(function_table),NULL,function_name,CIM,2,canvas_params TSRMLS_CC); 
      
    ZVAL_ZVAL(&resize_params[0], CIM, 1, 0);
    ZVAL_ZVAL(&resize_params[1], IM_CANVAS, 1, 0);
    ZVAL_LONG(&resize_params[2],0);
    ZVAL_LONG(&resize_params[3],0);
    ZVAL_LONG(&resize_params[4],*(crops+topCrop));
    ZVAL_LONG(&resize_params[5],*(crops+topCrop+1));
    ZVAL_LONG(&resize_params[6],DW);
    ZVAL_LONG(&resize_params[7],DH);
    ZVAL_LONG(&resize_params[8],DW);
    ZVAL_LONG(&resize_params[9],DH);
    ZVAL_STRING(function_name, "imagecopyresampled");
    call_user_function(EG(function_table),NULL,function_name,retval_ptr,10,resize_params TSRMLS_CC);

    RETURN_ZVAL(CIM,1,0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_smartcrop_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_smartcrop_init_globals(zend_smartcrop_globals *smartcrop_globals)
{
	smartcrop_globals->global_value = 0;
	smartcrop_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(smartcrop)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(smartcrop)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(smartcrop)
{
#if defined(COMPILE_DL_SMARTCROP) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(smartcrop)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(smartcrop)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "smartcrop support", "enabled");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */

/* {{{ smartcrop_functions[]
 *
 * Every user visible function must have an entry in smartcrop_functions[].
 */
const zend_function_entry smartcrop_functions[] = {
    PHP_FE(smartcrop,NULL)      /* For testing, remove later. */
    PHP_FE_END  /* Must be the last line in smartcrop_functions[] */
};
/* }}} */

/* {{{ smartcrop_module_entry
 */
zend_module_entry smartcrop_module_entry = {
    STANDARD_MODULE_HEADER,
    "smartcrop",
    smartcrop_functions,
    PHP_MINIT(smartcrop),
    PHP_MSHUTDOWN(smartcrop),
    PHP_RINIT(smartcrop),       /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(smartcrop),   /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(smartcrop),
    PHP_SMARTCROP_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SMARTCROP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(smartcrop)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
