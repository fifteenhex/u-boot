/*
 */

#ifndef CACHE_MC68040_H_
#define CACHE_MC68040_H_

void icache_enable_mc68040(void);
void icache_disable_mc68040(void);
int icache_status_mc68040(void);
void dcache_enable_mc68040(void);
void dcache_disable_mc68040(void);
int dcache_status_mc68040(void);

#endif /* CACHE_MC68040_H_ */
