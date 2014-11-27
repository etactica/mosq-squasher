/* 
 * File:   mosq_manage.h
 * Author: karlp
 *
 * Created on November 26, 2014, 2:39 PM
 */

#ifndef MOSQ_MANAGE_H
#define	MOSQ_MANAGE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "app.h"

	bool mosq_setup(struct _squash *st);
	void mosq_cleanup(struct _squash *st);


#ifdef	__cplusplus
}
#endif

#endif	/* MOSQ_MANAGE_H */

