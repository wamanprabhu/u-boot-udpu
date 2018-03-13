
#include <common.h>
#include <net.h>
#include <netdev.h>
#include <config.h>
#include <malloc.h>
#include <asm/io.h>
#include <phy.h>
#include <miiphy.h>
#include <watchdog.h>
#include <linux/compat.h>
#include <linux/mbus.h>
#include <fdtdec.h>
#include <fdt.h>
#include <pci.h>
#include <config/arch/mvebu.h>
#include <command.h>

#define SMI_CMD			0x00
#define SMI_CMD_BUSY		BIT(15)
#define SMI_CMD_CLAUSE_22	BIT(12)
#define SMI_CMD_OP_22_WRITE	((1 << 10) | SMI_CMD_BUSY | SMI_CMD_CLAUSE_22)
#define SMI_CMD_OP_22_READ	((2 << 10) | SMI_CMD_BUSY | SMI_CMD_CLAUSE_22)
#define SMI_CMD_OP_45_WRITE_ADDR	((0 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_WRITE_DATA	((1 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_READ_DATA		((2 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_OP_45_READ_DATA_INC	((3 << 10) | SMI_CMD_BUSY)
#define SMI_CMD_DEV_ADDR_MASK 0x1f
#define SMI_CMD_DEV_ADDR_SIZE 5
#define SMI_CMD_REG_ADDR_MASK 0x1f
#define SMI_DATA		0x01

#define SMI_PHY_PAGE_REG 0x16

/* Fiber/SERDES Registers are located at SMI address F, page 1 */
#define REG_FIBER_SERDES	0x0f
#define PAGE_FIBER_SERDES	0x01

#define REG_PORT_BASE_LEGACY	0x10
#define REG_PORT_BASE_PERIDOT	0
#define REG_PORT(p)		(REG_PORT_BASE + (p))
#define PORT_STATUS		0x00
#define PORT_STATUS_PAUSE_EN	BIT(15)
#define PORT_STATUS_MY_PAUSE	BIT(14)
#define PORT_STATUS_HD_FLOW	BIT(13)
#define PORT_STATUS_PHY_DETECT	BIT(12)
#define PORT_STATUS_LINK	BIT(11)
#define PORT_STATUS_DUPLEX	BIT(10)
#define PORT_STATUS_SPEED_MASK	0x0300
#define PORT_STATUS_SPEED_10	0x0000
#define PORT_STATUS_SPEED_100	0x0100
#define PORT_STATUS_SPEED_1000	0x0200
#define PORT_STATUS_EEE		BIT(6) /* 6352 */
#define PORT_STATUS_AM_DIS	BIT(6) /* 6165 */
#define PORT_STATUS_MGMII	BIT(6) /* 6185 */
#define PORT_STATUS_TX_PAUSED	BIT(5)
#define PORT_STATUS_FLOW_CTRL	BIT(4)
#define PORT_STATUS_CMODE_MASK	0x0f
#define PORT_STATUS_CMODE_100BASE_X	0x8
#define PORT_STATUS_CMODE_1000BASE_X	0x9
#define PORT_STATUS_CMODE_SGMII		0xa
#define PORT_PCS_CTRL		0x01
#define PORT_PCS_CTRL_RGMII_DELAY_RXCLK	BIT(15)
#define PORT_PCS_CTRL_RGMII_DELAY_TXCLK	BIT(14)
#define PORT_PCS_CTRL_FORCE_SPEED	BIT(13)
#define PORT_PCS_CTRL_FC		BIT(7)
#define PORT_PCS_CTRL_FORCE_FC		BIT(6)
#define PORT_PCS_CTRL_LINK_UP		BIT(5)
#define PORT_PCS_CTRL_FORCE_LINK	BIT(4)
#define PORT_PCS_CTRL_DUPLEX_FULL	BIT(3)
#define PORT_PCS_CTRL_FORCE_DUPLEX	BIT(2)
#define PORT_PCS_CTRL_10		0x00
#define PORT_PCS_CTRL_100		0x01
#define PORT_PCS_CTRL_1000		0x02
#define PORT_PCS_CTRL_UNFORCED		0x03
#define PORT_PAUSE_CTRL		0x02
#define PORT_SWITCH_ID		0x03

#define PORT_SWITCH_ID_PROD_NUM_6190	0x190
#define PORT_SWITCH_ID_PROD_NUM_6290	0x290
#define PORT_SWITCH_ID_PROD_NUM_6390	0x390
#define PORT_SWITCH_ID_PROD_NUM_6141	0x141
#define PORT_SWITCH_ID_PROD_NUM_6341	0x341

#define PORT_CONTROL		0x04
#define PORT_CONTROL_USE_CORE_TAG	BIT(15)
#define PORT_CONTROL_DROP_ON_LOCK	BIT(14)
#define PORT_CONTROL_EGRESS_UNMODIFIED	(0x0 << 12)
#define PORT_CONTROL_EGRESS_UNTAGGED	(0x1 << 12)
#define PORT_CONTROL_EGRESS_TAGGED	(0x2 << 12)
#define PORT_CONTROL_EGRESS_ADD_TAG	(0x3 << 12)
#define PORT_CONTROL_HEADER		BIT(11)
#define PORT_CONTROL_IGMP_MLD_SNOOP	BIT(10)
#define PORT_CONTROL_DOUBLE_TAG		BIT(9)
#define PORT_CONTROL_FRAME_MODE_NORMAL		(0x0 << 8)
#define PORT_CONTROL_FRAME_MODE_DSA		(0x1 << 8)
#define PORT_CONTROL_FRAME_MODE_PROVIDER	(0x2 << 8)
#define PORT_CONTROL_FRAME_ETHER_TYPE_DSA	(0x3 << 8)
#define PORT_CONTROL_DSA_TAG		BIT(8)
#define PORT_CONTROL_VLAN_TUNNEL	BIT(7)
#define PORT_CONTROL_TAG_IF_BOTH	BIT(6)
#define PORT_CONTROL_USE_IP		BIT(5)
#define PORT_CONTROL_USE_TAG		BIT(4)
#define PORT_CONTROL_FORWARD_UNKNOWN_MC	BIT(3)
#define PORT_CONTROL_FORWARD_UNKNOWN	BIT(2)
#define PORT_CONTROL_STATE_MASK		0x03
#define PORT_CONTROL_STATE_DISABLED	0x00
#define PORT_CONTROL_STATE_BLOCKING	0x01
#define PORT_CONTROL_STATE_LEARNING	0x02
#define PORT_CONTROL_STATE_FORWARDING	0x03
#define PORT_CONTROL_1		0x05
#define PORT_CONTROL_1_FID_11_4_MASK	(0xff << 0)
#define PORT_BASE_VLAN		0x06
#define PORT_BASE_VLAN_FID_3_0_MASK	(0xf << 12)
#define PORT_DEFAULT_VLAN	0x07
#define PORT_DEFAULT_VLAN_MASK	0xfff
#define PORT_CONTROL_2		0x08
#define PORT_CONTROL_2_IGNORE_FCS	BIT(15)
#define PORT_CONTROL_2_VTU_PRI_OVERRIDE	BIT(14)
#define PORT_CONTROL_2_SA_PRIO_OVERRIDE	BIT(13)
#define PORT_CONTROL_2_DA_PRIO_OVERRIDE	BIT(12)
#define PORT_CONTROL_2_JUMBO_1522	(0x00 << 12)
#define PORT_CONTROL_2_JUMBO_2048	(0x01 << 12)
#define PORT_CONTROL_2_JUMBO_10240	(0x02 << 12)
#define PORT_CONTROL_2_8021Q_MASK	(0x03 << 10)
#define PORT_CONTROL_2_8021Q_DISABLED	(0x00 << 10)
#define PORT_CONTROL_2_8021Q_FALLBACK	(0x01 << 10)
#define PORT_CONTROL_2_8021Q_CHECK	(0x02 << 10)
#define PORT_CONTROL_2_8021Q_SECURE	(0x03 << 10)
#define PORT_CONTROL_2_DISCARD_TAGGED	BIT(9)
#define PORT_CONTROL_2_DISCARD_UNTAGGED	BIT(8)
#define PORT_CONTROL_2_MAP_DA		BIT(7)
#define PORT_CONTROL_2_DEFAULT_FORWARD	BIT(6)
#define PORT_CONTROL_2_FORWARD_UNKNOWN	BIT(6)
#define PORT_CONTROL_2_EGRESS_MONITOR	BIT(5)
#define PORT_CONTROL_2_INGRESS_MONITOR	BIT(4)
#define PORT_RATE_CONTROL	0x09
#define PORT_RATE_CONTROL_2	0x0a
#define PORT_ASSOC_VECTOR	0x0b
#define PORT_ASSOC_VECTOR_HOLD_AT_1		BIT(15)
#define PORT_ASSOC_VECTOR_INT_AGE_OUT		BIT(14)
#define PORT_ASSOC_VECTOR_LOCKED_PORT		BIT(13)
#define PORT_ASSOC_VECTOR_IGNORE_WRONG		BIT(12)
#define PORT_ASSOC_VECTOR_REFRESH_LOCKED	BIT(11)
#define PORT_ATU_CONTROL	0x0c
#define PORT_PRI_OVERRIDE	0x0d
#define PORT_ETH_TYPE		0x0f
#define PORT_IN_DISCARD_LO	0x10
#define PORT_IN_DISCARD_HI	0x11
#define PORT_IN_FILTERED	0x12
#define PORT_OUT_FILTERED	0x13
#define PORT_TAG_REGMAP_0123	0x18
#define PORT_TAG_REGMAP_4567	0x19

#define REG_GLOBAL		0x1b
#define GLOBAL_STATUS		0x00
#define GLOBAL_STATUS_PPU_STATE BIT(15) /* 6351 and 6171 */
/* Two bits for 6165, 6185 etc */
#define GLOBAL_STATUS_PPU_MASK		(0x3 << 14)
#define GLOBAL_STATUS_PPU_DISABLED_RST	(0x0 << 14)
#define GLOBAL_STATUS_PPU_INITIALIZING	(0x1 << 14)
#define GLOBAL_STATUS_PPU_DISABLED	(0x2 << 14)
#define GLOBAL_STATUS_PPU_POLLING	(0x3 << 14)
#define GLOBAL_MAC_01		0x01
#define GLOBAL_MAC_23		0x02
#define GLOBAL_MAC_45		0x03
#define GLOBAL_ATU_FID		0x01	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_FID		0x02	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_FID_MASK	0xfff
#define GLOBAL_VTU_SID		0x03	/* 6097 6165 6351 6352 */
#define GLOBAL_VTU_SID_MASK	0x3f
#define GLOBAL_CONTROL		0x04
#define GLOBAL_CONTROL_SW_RESET		BIT(15)
#define GLOBAL_CONTROL_PPU_ENABLE	BIT(14)
#define GLOBAL_CONTROL_DISCARD_EXCESS	BIT(13) /* 6352 */
#define GLOBAL_CONTROL_SCHED_PRIO	BIT(11) /* 6152 */
#define GLOBAL_CONTROL_MAX_FRAME_1632	BIT(10) /* 6152 */
#define GLOBAL_CONTROL_RELOAD_EEPROM	BIT(9)	/* 6152 */
#define GLOBAL_CONTROL_DEVICE_EN	BIT(7)
#define GLOBAL_CONTROL_STATS_DONE_EN	BIT(6)
#define GLOBAL_CONTROL_VTU_PROBLEM_EN	BIT(5)
#define GLOBAL_CONTROL_VTU_DONE_EN	BIT(4)
#define GLOBAL_CONTROL_ATU_PROBLEM_EN	BIT(3)
#define GLOBAL_CONTROL_ATU_DONE_EN	BIT(2)
#define GLOBAL_CONTROL_TCAM_EN		BIT(1)
#define GLOBAL_CONTROL_EEPROM_DONE_EN	BIT(0)
#define GLOBAL_VTU_OP		0x05
#define GLOBAL_VTU_OP_BUSY	BIT(15)
#define GLOBAL_VTU_OP_FLUSH_ALL		((0x01 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_VTU_LOAD_PURGE	((0x03 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_VTU_GET_NEXT	((0x04 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_STU_LOAD_PURGE	((0x05 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_OP_STU_GET_NEXT	((0x06 << 12) | GLOBAL_VTU_OP_BUSY)
#define GLOBAL_VTU_VID		0x06
#define GLOBAL_VTU_VID_MASK	0xfff
#define GLOBAL_VTU_VID_VALID	BIT(12)
#define GLOBAL_VTU_DATA_0_3	0x07
#define GLOBAL_VTU_DATA_4_7	0x08
#define GLOBAL_VTU_DATA_8_11	0x09
#define GLOBAL_VTU_STU_DATA_MASK		0x03
#define GLOBAL_VTU_DATA_MEMBER_TAG_UNMODIFIED	0x00
#define GLOBAL_VTU_DATA_MEMBER_TAG_UNTAGGED	0x01
#define GLOBAL_VTU_DATA_MEMBER_TAG_TAGGED	0x02
#define GLOBAL_VTU_DATA_MEMBER_TAG_NON_MEMBER	0x03
#define GLOBAL_STU_DATA_PORT_STATE_DISABLED	0x00
#define GLOBAL_STU_DATA_PORT_STATE_BLOCKING	0x01
#define GLOBAL_STU_DATA_PORT_STATE_LEARNING	0x02
#define GLOBAL_STU_DATA_PORT_STATE_FORWARDING	0x03
#define GLOBAL_ATU_CONTROL	0x0a
#define GLOBAL_ATU_CONTROL_LEARN2ALL	BIT(3)
#define GLOBAL_ATU_OP		0x0b
#define GLOBAL_ATU_OP_BUSY	BIT(15)
#define GLOBAL_ATU_OP_NOP		(0 << 12)
#define GLOBAL_ATU_OP_FLUSH_MOVE_ALL		((1 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_NON_STATIC	((2 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_LOAD_DB		((3 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_GET_NEXT_DB	((4 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_ALL_DB		((5 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_FLUSH_MOVE_NON_STATIC_DB ((6 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_OP_GET_CLR_VIOLATION	  ((7 << 12) | GLOBAL_ATU_OP_BUSY)
#define GLOBAL_ATU_DATA		0x0c
#define GLOBAL_ATU_DATA_TRUNK			BIT(15)
#define GLOBAL_ATU_DATA_TRUNK_ID_MASK		0x00f0
#define GLOBAL_ATU_DATA_TRUNK_ID_SHIFT		4
#define GLOBAL_ATU_DATA_PORT_VECTOR_MASK	0x3ff0
#define GLOBAL_ATU_DATA_PORT_VECTOR_SHIFT	4
#define GLOBAL_ATU_DATA_STATE_MASK		0x0f
#define GLOBAL_ATU_DATA_STATE_UNUSED		0x00
#define GLOBAL_ATU_DATA_STATE_UC_MGMT		0x0d
#define GLOBAL_ATU_DATA_STATE_UC_STATIC		0x0e
#define GLOBAL_ATU_DATA_STATE_UC_PRIO_OVER	0x0f
#define GLOBAL_ATU_DATA_STATE_MC_NONE_RATE	0x05
#define GLOBAL_ATU_DATA_STATE_MC_STATIC		0x07
#define GLOBAL_ATU_DATA_STATE_MC_MGMT		0x0e
#define GLOBAL_ATU_DATA_STATE_MC_PRIO_OVER	0x0f
#define GLOBAL_ATU_MAC_01	0x0d
#define GLOBAL_ATU_MAC_23	0x0e
#define GLOBAL_ATU_MAC_45	0x0f
#define GLOBAL_IP_PRI_0		0x10
#define GLOBAL_IP_PRI_1		0x11
#define GLOBAL_IP_PRI_2		0x12
#define GLOBAL_IP_PRI_3		0x13
#define GLOBAL_IP_PRI_4		0x14
#define GLOBAL_IP_PRI_5		0x15
#define GLOBAL_IP_PRI_6		0x16
#define GLOBAL_IP_PRI_7		0x17
#define GLOBAL_IEEE_PRI		0x18
#define GLOBAL_CORE_TAG_TYPE	0x19
#define GLOBAL_MONITOR_CONTROL	0x1a
#define GLOBAL_MONITOR_CONTROL_INGRESS_SHIFT	12
#define GLOBAL_MONITOR_CONTROL_EGRESS_SHIFT	8
#define GLOBAL_MONITOR_CONTROL_ARP_SHIFT	4
#define GLOBAL_MONITOR_CONTROL_MIRROR_SHIFT	0
#define GLOBAL_MONITOR_CONTROL_ARP_DISABLED	(0xf0)
#define GLOBAL_CONTROL_2	0x1c
#define GLOBAL_CONTROL_2_NO_CASCADE		0xe000
#define GLOBAL_CONTROL_2_MULTIPLE_CASCADE	0xf000

#define GLOBAL_STATS_OP		0x1d
#define GLOBAL_STATS_OP_BUSY	BIT(15)
#define GLOBAL_STATS_OP_NOP		(0 << 12)
#define GLOBAL_STATS_OP_FLUSH_ALL	((1 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_FLUSH_PORT	((2 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_READ_CAPTURED	((4 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_CAPTURE_PORT	((5 << 12) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_RX		((1 << 10) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_TX		((2 << 10) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_HIST_RX_TX	((3 << 10) | GLOBAL_STATS_OP_BUSY)
#define GLOBAL_STATS_OP_BANK_1	BIT(9)
#define GLOBAL_STATS_COUNTER_32	0x1e
#define GLOBAL_STATS_COUNTER_01	0x1f

#define REG_GLOBAL2		0x1c
#define GLOBAL2_INT_SOURCE	0x00
#define GLOBAL2_INT_MASK	0x01
#define GLOBAL2_MGMT_EN_2X	0x02
#define GLOBAL2_MGMT_EN_0X	0x03
#define GLOBAL2_FLOW_CONTROL	0x04
#define GLOBAL2_SWITCH_MGMT	0x05
#define GLOBAL2_SWITCH_MGMT_USE_DOUBLE_TAG_DATA	BIT(15)
#define GLOBAL2_SWITCH_MGMT_PREVENT_LOOPS	BIT(14)
#define GLOBAL2_SWITCH_MGMT_FLOW_CONTROL_MSG	BIT(13)
#define GLOBAL2_SWITCH_MGMT_FORCE_FLOW_CTRL_PRI	BIT(7)
#define GLOBAL2_SWITCH_MGMT_RSVD2CPU		BIT(3)
#define GLOBAL2_DEVICE_MAPPING	0x06
#define GLOBAL2_DEVICE_MAPPING_UPDATE		BIT(15)
#define GLOBAL2_DEVICE_MAPPING_TARGET_SHIFT	8
#define GLOBAL2_DEVICE_MAPPING_PORT_MASK	0x0f
#define GLOBAL2_TRUNK_MASK	0x07
#define GLOBAL2_TRUNK_MASK_UPDATE		BIT(15)
#define GLOBAL2_TRUNK_MASK_NUM_SHIFT		12
#define GLOBAL2_TRUNK_MAPPING	0x08
#define GLOBAL2_TRUNK_MAPPING_UPDATE		BIT(15)
#define GLOBAL2_TRUNK_MAPPING_ID_SHIFT		11
#define GLOBAL2_INGRESS_OP	0x09
#define GLOBAL2_INGRESS_DATA	0x0a
#define GLOBAL2_PVT_ADDR	0x0b
#define GLOBAL2_PVT_DATA	0x0c
#define GLOBAL2_SWITCH_MAC	0x0d
#define GLOBAL2_SWITCH_MAC_BUSY BIT(15)
#define GLOBAL2_ATU_STATS	0x0e
#define GLOBAL2_PRIO_OVERRIDE	0x0f
#define GLOBAL2_PRIO_OVERRIDE_FORCE_SNOOP	BIT(7)
#define GLOBAL2_PRIO_OVERRIDE_SNOOP_SHIFT	4
#define GLOBAL2_PRIO_OVERRIDE_FORCE_ARP		BIT(3)
#define GLOBAL2_PRIO_OVERRIDE_ARP_SHIFT		0
#define GLOBAL2_EEPROM_OP	0x14
#define GLOBAL2_EEPROM_OP_BUSY		BIT(15)
#define GLOBAL2_EEPROM_OP_WRITE		((3 << 12) | GLOBAL2_EEPROM_OP_BUSY)
#define GLOBAL2_EEPROM_OP_READ		((4 << 12) | GLOBAL2_EEPROM_OP_BUSY)
#define GLOBAL2_EEPROM_OP_LOAD		BIT(11)
#define GLOBAL2_EEPROM_OP_WRITE_EN	BIT(10)
#define GLOBAL2_EEPROM_OP_ADDR_MASK	0xff
#define GLOBAL2_EEPROM_DATA	0x15
#define GLOBAL2_PTP_AVB_OP	0x16
#define GLOBAL2_PTP_AVB_DATA	0x17
#define GLOBAL2_SMI_OP		0x18
#define GLOBAL2_SMI_OP_BUSY		BIT(15)
#define GLOBAL2_SMI_OP_CLAUSE_22	BIT(12)
#define GLOBAL2_SMI_OP_22_WRITE		((1 << 10) | GLOBAL2_SMI_OP_BUSY | \
					 GLOBAL2_SMI_OP_CLAUSE_22)
#define GLOBAL2_SMI_OP_22_READ		((2 << 10) | GLOBAL2_SMI_OP_BUSY | \
					 GLOBAL2_SMI_OP_CLAUSE_22)
#define GLOBAL2_SMI_OP_45_WRITE_ADDR	((0 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_OP_45_WRITE_DATA	((1 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_OP_45_READ_DATA	((2 << 10) | GLOBAL2_SMI_OP_BUSY)
#define GLOBAL2_SMI_DATA	0x19
#define GLOBAL2_SCRATCH_MISC	0x1a
#define GLOBAL2_SCRATCH_BUSY		BIT(15)
#define GLOBAL2_SCRATCH_REGISTER_SHIFT	8
#define GLOBAL2_SCRATCH_VALUE_MASK	0xff
#define GLOBAL2_WDOG_CONTROL	0x1b
#define GLOBAL2_QOS_WEIGHT	0x1c
#define GLOBAL2_MISC		0x1d

#define PHY_COPPER_CONTROL		0x0
#define PHY_COPPER_CONTROL_SPEED_1G	BIT(6)
#define PHY_COPPER_CONTROL_DUPLEX	BIT(8)
#define PHY_COPPER_CONTROL_POWER_DOWN	BIT(11)
#define PHY_COPPER_CONTROL_AUTO_NEG_EN	BIT(12)
#define PHY_COPPER_CONTROL_SPEED_100M	BIT(13)

struct mv88e6xxx_dev {
	unsigned int phy_addr;
	unsigned int port_mask;
	unsigned int addr_mode;
	int id;
	int cpu_port;	/* The switch port to which the CPU is attached */
};

enum sw_opts {
	SW_INFO,
	SW_READ,
	SW_WRITE,
	SW_PHY_READ,
	SW_PHY_WRITE,
	SW_LINK,
	SW_NA
};
