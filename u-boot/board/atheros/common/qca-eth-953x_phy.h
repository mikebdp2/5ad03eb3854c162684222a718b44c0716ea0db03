#ifndef _QCA_ETH_953x_PHY_H
#define _QCA_ETH_953x_PHY_H
#include <miiphy.h>


#ifdef CONFIG_ATHR_8033_PHY
extern int athrs_ar8033_reg_init(void *arg);
extern int athrs_ar8033_phy_setup(void  *arg);
extern int athrs_ar8033_phy_is_fdx(int ethUnit);
extern int athrs_ar8033_phy_is_link_alive(int phyUnit);
extern int athrs_ar8033_phy_is_up(int ethUnit);
extern int athrs_ar8033_phy_speed(int ethUnit,int phyUnit);
#endif

#ifdef CFG_ATHRS27_PHY
extern int athrs27_phy_setup(int ethUnit);
extern int athrs27_phy_is_up(int ethUnit);
extern int athrs27_phy_is_fdx(int ethUnit);
extern int athrs27_phy_speed(int ethUnit);
#endif

#ifdef CONFIG_ATHRS17_PHY
extern int athrs17_phy_setup(int ethUnit);
extern int athrs17_phy_is_up(int ethUnit);
extern int athrs17_phy_is_fdx(int ethUnit);
extern int athrs17_phy_speed(int ethUnit);
#endif

static inline void ath_gmac_phy_setup(int unit)
{
#ifdef CFG_ATHRS27_PHY
                        athrs27_phy_setup(unit);
#endif
#ifdef CONFIG_VIR_PHY
			athr_vir_phy_setup(unit);
#endif
}

static inline void ath_gmac_phy_link(int unit, int *link)
{

#ifdef CFG_ATHRS27_PHY
                        *link = athrs27_phy_is_up(unit);
#endif

#ifdef CONFIG_VIR_PHY
			*link = athr_vir_phy_is_up(unit);
#endif
}

static inline void ath_gmac_phy_duplex(int unit, int *duplex)
{
#ifdef CFG_ATHRS27_PHY
                        *duplex = athrs27_phy_is_fdx(unit);
#endif
#ifdef CONFIG_VIR_PHY
			*duplex = athr_vir_phy_is_fdx(unit);
#endif
}

static inline void ath_gmac_phy_speed(int unit, int *speed)
{
#ifdef CFG_ATHRS27_PHY
			 *speed = athrs27_phy_speed(unit);
#endif

#ifdef CONFIG_VIR_PHY
			*speed = athr_vir_phy_speed(unit);
#endif

}

#endif /* _QCA_ETH_953x_PHY_H */
