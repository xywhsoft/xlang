#ifndef XGE_CONFIG_H
#define XGE_CONFIG_H

/* Use the same configuration in the library and every consumer. */
#define XGE_PROFILE_DEFAULT 0
#define XGE_PROFILE_CORE 1
#define XGE_PROFILE_2D 2
#define XGE_PROFILE_3D 3
#define XGE_PROFILE_UI_MIN 4
#define XGE_PROFILE_FULL_DEV 5
#ifndef XGE_BUILD_PROFILE
#define XGE_BUILD_PROFILE XGE_PROFILE_DEFAULT
#endif
#if XGE_BUILD_PROFILE < 0 || XGE_BUILD_PROFILE > XGE_PROFILE_FULL_DEV
#error "Unknown XGE_BUILD_PROFILE"
#endif

#ifndef XGE_ENABLE_2D
#define XGE_ENABLE_2D (XGE_BUILD_PROFILE != XGE_PROFILE_CORE && XGE_BUILD_PROFILE != XGE_PROFILE_3D)
#endif
#ifndef XGE_ENABLE_3D
#define XGE_ENABLE_3D (XGE_BUILD_PROFILE == XGE_PROFILE_3D || XGE_BUILD_PROFILE == XGE_PROFILE_FULL_DEV)
#endif
#ifndef XGE_ENABLE_TEXT
#ifdef XGE_NO_TEXT
#define XGE_ENABLE_TEXT 0
#else
#define XGE_ENABLE_TEXT (XGE_ENABLE_2D && XGE_BUILD_PROFILE != XGE_PROFILE_2D)
#endif
#endif
#ifndef XGE_ENABLE_AUDIO
#ifdef XGE_NO_AUDIO
#define XGE_ENABLE_AUDIO 0
#else
#define XGE_ENABLE_AUDIO (XGE_BUILD_PROFILE == XGE_PROFILE_DEFAULT || XGE_BUILD_PROFILE == XGE_PROFILE_FULL_DEV)
#endif
#endif
#ifndef XGE_ENABLE_XUI
#define XGE_ENABLE_XUI (XGE_ENABLE_2D && XGE_ENABLE_TEXT)
#endif
#ifndef XGE_ENABLE_SHAPE_EX
#define XGE_ENABLE_SHAPE_EX (XGE_ENABLE_2D && XGE_BUILD_PROFILE != XGE_PROFILE_2D)
#endif
#ifndef XGE_ENABLE_SVG
#define XGE_ENABLE_SVG (XGE_ENABLE_SHAPE_EX && XGE_BUILD_PROFILE != XGE_PROFILE_UI_MIN)
#endif
#ifndef XGE_ENABLE_EMOJI
#define XGE_ENABLE_EMOJI (XGE_ENABLE_TEXT && XGE_ENABLE_SVG)
#endif
#ifndef XGE_ENABLE_PARTICLES
#define XGE_ENABLE_PARTICLES (XGE_ENABLE_2D && XGE_BUILD_PROFILE != XGE_PROFILE_2D && XGE_BUILD_PROFILE != XGE_PROFILE_UI_MIN)
#endif
#ifndef XGE_ENABLE_HARFBUZZ
#define XGE_ENABLE_HARFBUZZ 0
#endif

#if defined(XGE_NO_TEXT) && XGE_ENABLE_TEXT
#error "XGE_NO_TEXT conflicts with XGE_ENABLE_TEXT"
#endif
#if defined(XGE_NO_AUDIO) && XGE_ENABLE_AUDIO
#error "XGE_NO_AUDIO conflicts with XGE_ENABLE_AUDIO"
#endif
#if !XGE_ENABLE_TEXT && !defined(XGE_NO_TEXT)
#define XGE_NO_TEXT 1
#endif
#if !XGE_ENABLE_AUDIO && !defined(XGE_NO_AUDIO)
#define XGE_NO_AUDIO 1
#endif

#if XGE_ENABLE_XUI && (!XGE_ENABLE_2D || !XGE_ENABLE_TEXT)
#error "XUI requires 2D and text"
#endif
#if XGE_ENABLE_TEXT && !XGE_ENABLE_2D
#error "Text requires 2D"
#endif
#if XGE_ENABLE_SHAPE_EX && !XGE_ENABLE_2D
#error "ShapeEx requires 2D"
#endif
#if XGE_ENABLE_SVG && !XGE_ENABLE_SHAPE_EX
#error "SVG requires ShapeEx"
#endif
#if XGE_ENABLE_EMOJI && (!XGE_ENABLE_TEXT || !XGE_ENABLE_SVG)
#error "Emoji requires text and SVG"
#endif
#if XGE_ENABLE_HARFBUZZ && !XGE_ENABLE_TEXT
#error "HarfBuzz requires text"
#endif
#if XGE_ENABLE_PARTICLES && !XGE_ENABLE_2D
#error "The existing particle renderer requires 2D"
#endif

#ifndef XGE3D_ENABLE_MODEL
#define XGE3D_ENABLE_MODEL XGE_ENABLE_3D
#endif
#ifndef XGE3D_ENABLE_LIGHTING
#define XGE3D_ENABLE_LIGHTING XGE_ENABLE_3D
#endif
#ifndef XGE3D_ENABLE_SHADOW
#define XGE3D_ENABLE_SHADOW XGE3D_ENABLE_LIGHTING
#endif
#ifndef XGE3D_ENABLE_ANIMATION
#define XGE3D_ENABLE_ANIMATION XGE3D_ENABLE_MODEL
#endif
#ifndef XGE3D_ENABLE_IBL
#define XGE3D_ENABLE_IBL XGE3D_ENABLE_LIGHTING
#endif
#ifndef XGE3D_ENABLE_TERRAIN
#define XGE3D_ENABLE_TERRAIN XGE_ENABLE_3D
#endif
#ifndef XGE3D_ENABLE_ASYNC
#define XGE3D_ENABLE_ASYNC XGE3D_ENABLE_MODEL
#endif
#ifndef XGE3D_ENABLE_FOG
#define XGE3D_ENABLE_FOG XGE_ENABLE_3D
#endif
#if (XGE3D_ENABLE_FOG != 0 && XGE3D_ENABLE_FOG != 1) || (XGE3D_ENABLE_FOG && !XGE_ENABLE_3D)
#error "3D fog requires 3D and a 0 or 1 feature switch"
#endif
#if XGE3D_ENABLE_ASYNC && (!XGE3D_ENABLE_MODEL || !XGE_ENABLE_3D)
#error "3D asynchronous loading requires 3D models"
#endif
#if XGE3D_ENABLE_ASYNC != 0 && XGE3D_ENABLE_ASYNC != 1
#error "XGE feature switches must be 0 or 1"
#endif
#if !XGE_ENABLE_3D && (XGE3D_ENABLE_MODEL || XGE3D_ENABLE_LIGHTING || XGE3D_ENABLE_SHADOW || XGE3D_ENABLE_ANIMATION || XGE3D_ENABLE_IBL || XGE3D_ENABLE_TERRAIN)
#error "3D extensions require XGE_ENABLE_3D"
#endif
#if XGE3D_ENABLE_SHADOW && !XGE3D_ENABLE_LIGHTING
#error "Shadows require 3D lighting"
#endif
#if XGE3D_ENABLE_IBL && !XGE3D_ENABLE_LIGHTING
#error "IBL requires 3D lighting"
#endif
#if XGE3D_ENABLE_ANIMATION && !XGE3D_ENABLE_MODEL
#error "Animation requires 3D models"
#endif

#define XGE_FEATURES ((unsigned int)(XGE_ENABLE_2D) | ((unsigned int)(XGE_ENABLE_3D) << 1) | \
    ((unsigned int)(XGE_ENABLE_SHAPE_EX) << 2) | ((unsigned int)(XGE_ENABLE_XUI) << 3) | \
    ((unsigned int)(XGE_ENABLE_TEXT) << 4) | ((unsigned int)(XGE_ENABLE_AUDIO) << 5) | \
    ((unsigned int)(XGE_ENABLE_SVG) << 6) | ((unsigned int)(XGE_ENABLE_PARTICLES) << 7) | \
    ((unsigned int)(XGE_ENABLE_EMOJI) << 8) | ((unsigned int)(XGE_ENABLE_HARFBUZZ) << 9))


#if (XGE3D_ENABLE_ANIMATION != 0 && XGE3D_ENABLE_ANIMATION != 1) || (XGE3D_ENABLE_IBL != 0 && XGE3D_ENABLE_IBL != 1) || (XGE3D_ENABLE_LIGHTING != 0 && XGE3D_ENABLE_LIGHTING != 1) || (XGE3D_ENABLE_MODEL != 0 && XGE3D_ENABLE_MODEL != 1) || (XGE3D_ENABLE_SHADOW != 0 && XGE3D_ENABLE_SHADOW != 1) || (XGE3D_ENABLE_TERRAIN != 0 && XGE3D_ENABLE_TERRAIN != 1) || (XGE_ENABLE_2D != 0 && XGE_ENABLE_2D != 1) || (XGE_ENABLE_3D != 0 && XGE_ENABLE_3D != 1) || (XGE_ENABLE_AUDIO != 0 && XGE_ENABLE_AUDIO != 1) || (XGE_ENABLE_EMOJI != 0 && XGE_ENABLE_EMOJI != 1) || (XGE_ENABLE_HARFBUZZ != 0 && XGE_ENABLE_HARFBUZZ != 1) || (XGE_ENABLE_PARTICLES != 0 && XGE_ENABLE_PARTICLES != 1) || (XGE_ENABLE_SHAPE_EX != 0 && XGE_ENABLE_SHAPE_EX != 1) || (XGE_ENABLE_SVG != 0 && XGE_ENABLE_SVG != 1) || (XGE_ENABLE_TEXT != 0 && XGE_ENABLE_TEXT != 1) || (XGE_ENABLE_XUI != 0 && XGE_ENABLE_XUI != 1)
#error "XGE feature switches must be 0 or 1"
#endif
#endif
