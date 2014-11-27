/* 
 * Ugly, low performance, configurable level, logging "framework"
 * Karl Palsson, ReMake Electric ehf, 2011
 */

#ifndef UGLYLOGGING_H
#define	UGLYLOGGING_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define UDEBUG 90
#define UINFO  50
#define UWARN  30
#define UERROR 20
#define UFATAL 10

    /**
     * Deprecated!
     */
    int ugly_init(int maximum_threshold);

    int ugly_log(int level, const char *tag, const char *format, ...);

    /**
     * sets up logging for a given application name and threshold.
     * Will be syslog if the parent pid is 1, otherwise stderr
     */
    int ugly_init_named(int maxium_threshold, const char *name);


#ifdef	__cplusplus
}
#endif

#endif	/* UGLYLOGGING_H */

