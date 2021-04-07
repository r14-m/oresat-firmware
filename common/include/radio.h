/**
 * @file    radio.h
 * @brief   OreSat radio support library.
 *
 * @addtogroup RADIO
 * @ingroup ORESAT
 * @{
 */
#ifndef _RADIO_H_
#define _RADIO_H_

#include "ax5043.h"
#include "si41xx.h"
#include "frame_buf.h"

/*===========================================================================*/
/* Constants.                                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Pre-compile time settings.                                                */
/*===========================================================================*/

#if !defined(RADIO_FB_COUNT) || defined(__DOXYGEN__)
#define RADIO_FB_COUNT                      8U
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Data structures and types.                                                */
/*===========================================================================*/

typedef struct {
    SI41XXDriver            *devp;
    SI41XXConfig            *cfgp;
    const char              *name;
} synth_dev_t;

typedef struct {
    AX5043Driver            *devp;
    const AX5043Config      *cfgp;
    const char              *name;
} radio_dev_t;

typedef struct {
    const ax5043_profile_t  *profile;
    const char              *name;
} radio_profile_t;

/*===========================================================================*/
/* Macros.                                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

extern synth_dev_t synth_devices[];
extern radio_dev_t radio_devices[];
extern radio_profile_t radio_profiles[];

void radio_init(void);
void radio_start(void);
void radio_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* _RADIO_H_ */

/** @} */
