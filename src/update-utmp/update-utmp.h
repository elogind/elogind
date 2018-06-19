#pragma once
#ifndef ELOGIND_SRC_UPDATE_UTMP_UPDATE_UTMP_H_INCLUDED
#define ELOGIND_SRC_UPDATE_UTMP_UPDATE_UTMP_H_INCLUDED

/******************************************************************
* Make the old main() from update-utmp.c visible as update_utmp() *
******************************************************************/

void update_utmp(int argc, char* argv[]);


#endif // ELOGIND_SRC_UPDATE_UTMP_UPDATE_UTMP_H_INCLUDED
