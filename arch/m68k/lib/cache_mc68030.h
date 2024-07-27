/*
 */

#ifndef CACHE_MC68030_H_
#define CACHE_MC68030_H_

void icache_enable_mc68030(void);
void icache_disable_mc68030(void);
int icache_status_mc68030(void);
void dcache_enable_mc68030(void);
void dcache_disable_mc68030(void);
int dcache_status_mc68030(void);

#endif /* CACHE_MC68030_H_ */
