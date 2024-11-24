#include "hpet.h"
#include "../../includes/hardware.h"
#include "../../includes/log.h"
#include "../../includes/memory.h"
#include "../../includes/interrupt.h"
#include "../../includes/task.h"
#include "../../includes/smp.h"

static u32 _minTick = 0;
// number of tick pre millisecond
static u64 _tickPreMs = 0;

static IntrController _intrCotroller;
static IntrHandler _intrHandler;
static APICRteDescriptor _intrDesc;

static HPETDescriptor *_hpetDesc;
Atomic HPET_jiffies;

#define _CapId					0x0
#define _Cfg					0x10
#define _Cfg_LegacySupport 		(1ul << 1)
#define _Cfg_Enable				(1ul << 0)
#define _IntrStatus				0x20
#define _MainCounterValue 		0xf0
#define _TimerCfgCap			0x100
#define _TimerCfgCap_Enable		(1ul << 2)
#define _TimerCfgCap_Period		(1ul << 3)
#define _TimerCfgCap_PeriodCap	(1ul << 4)
#define _TimerCfgCap_64Cap		(1ul << 5)
#define _TimerCfgCap_SetVal		(1ul << 6)
#define _TimerCfgCap_32bit		(1ul << 8)
#define _TimerCfgCap_Irq(x)		(1ul << ((x) + 32))
#define _TimerCmpVal			0x108
#define _TimerFSBIntr			0x110

static __always_inline__ void _setCfgQuad(u64 offset, u64 val) { *(u64 *)DMAS_phys2Virt(_hpetDesc->address.Address + offset) = val; IO_mfence(); }
static __always_inline__ u64 _getCfgQuad(u64 offset) { return *(u64 *)DMAS_phys2Virt(_hpetDesc->address.Address + offset); }
static __always_inline__ void _setCfgDword(u64 offset, u32 val) { *(u32 *)DMAS_phys2Virt(_hpetDesc->address.Address + offset) = val; IO_mfence(); }
static __always_inline__ u32 _getCfgDword(u64 offset) { return *(u32 *)DMAS_phys2Virt(_hpetDesc->address.Address + offset); }

static __always_inline__ u64 _getTimerConfig(u32 id) { return _getCfgQuad(0x20 * id + _TimerCmpVal); }

static __always_inline__ void _setTimerConfig(u32 id, u64 config) {
	u64 readonlyPart = _getCfgQuad(_TimerCfgCap + 0x20 * id) & 0x8030;
	// focused to run in 32-bit mode
	_setCfgQuad(_TimerCfgCap + 0x20 * id, config | readonlyPart);
}
static __always_inline__ void _setTimerComparator(u32 id, u64 comparator) {
	_setCfgQuad(_TimerCmpVal + 0x20 * id, comparator);
}

static int _mode;

IntrHandlerDeclare(HW_Timer_HPET_handler) {
	// print the counter
	Atomic_inc(&HPET_jiffies);
	if (!Task_cfsStruct.flags) return 0;
	Task_updateAllProcessorState();
	printk(BLACK, WHITE, "H");
	return 0;
}

// this interrupt is for timer requires
IntrHandlerDeclare(HW_Timer_HPET_handler2) {

}

void HW_Timer_HPET_init() {
	printk(RED, BLACK, "HW_Timer_HPET_init()\n");
	// initializ the data structure
	HPET_jiffies.value = _mode = 0;
	// get XSDT address
	XSDTDescriptor *xsdt = HW_UEFI_getXSDT();
	// find HPET in XSDT
	_hpetDesc = NULL;
	for (i32 i = 0; i < xsdt->header.length; i++) {
		HPETDescriptor *desc = (HPETDescriptor *)DMAS_phys2Virt(xsdt->entry[i]);
		if (!strncmp(desc->signature, "HPET", 4)) {
			_hpetDesc = desc;
			break;
		}
	}

	IO_out32(0xcf8, 0x8000f8f0);
	u32 x = IO_in32(0xcfc) & 0xffffc000;
	if (x > 0xfec00000 && x < 0xfee00000) {
		printk(RED, WHITE, "x = %#010x\n", x);
		u32 *p = (u32 *)DMAS_phys2Virt(x + 0x3404ul);
		*p = 0x80;
		IO_mfence();
	} else printk(WHITE, BLACK, "No need to set enable register (x = %#010x)\n", x);

	if (_hpetDesc == NULL) {
		printk(RED, BLACK, "HPET not found\n");
		while (1) IO_hlt();
	} else {
		printk(WHITE, BLACK, "HPET found at %#018lx, address: %#018lx\n", _hpetDesc, _hpetDesc->address.Address);
	}
	
	// initialize controller
	_intrCotroller.install = HW_APIC_install;
	_intrCotroller.uninstall = HW_APIC_uninstall;

	_intrCotroller.enable = HW_APIC_enableIntr;
	_intrCotroller.disable = HW_APIC_disableIntr;
	_intrCotroller.ack = HW_APIC_edgeAck;

	// initialize handler
	_intrHandler = HW_Timer_HPET_handler;

	// initialize descriptor
	memset(&_intrDesc, 0, sizeof(APICRteDescriptor));
	_intrDesc.vector = 0x22;
	_intrDesc.deliveryMode = HW_APIC_DeliveryMode_Fixed;
	_intrDesc.destMode = HW_APIC_DestMode_Physical;
	_intrDesc.deliveryStatus = HW_APIC_DeliveryStatus_Idle;
	_intrDesc.pinPolarity = HW_APIC_PinPolarity_High;
	_intrDesc.remoteIRR = HW_APIC_RemoteIRR_Reset;
	_intrDesc.triggerMode = HW_APIC_TriggerMode_Edge;
	_intrDesc.mask = HW_APIC_Mask_Masked;

	// set the destination as the current CPU
	{
		u32 a, b, c, d;
		HW_CPU_cpuid(0x1, 0, &a, &b, &c, &d);
		// I dont know why I need to use logical destination, but it can run on Intel Ultra 7 155H, so I keep it like this.
		_intrDesc.destDesc.logical.destination = SMP_getCPUInfoPkg(SMP_bspIdx())->x2apicID; 
		printk(WHITE, BLACK, "HPET: timer interrupt at processor with apicID=%x\n", SMP_getCPUInfoPkg(SMP_bspIdx())->x2apicID);
	}

	// set the general configuration register
	u64 cReg = _getCfgQuad(_CapId);
	printk(YELLOW, BLACK, "HPET: Capability: %#018lx \t", cReg);
	if (cReg & 0x2000) printk(WHITE, BLACK, "64-bit supply:Y \t");
	else printk(WHITE, BLACK, "64-bit supply:N \t");
	if (cReg & 0x8000) printk(WHITE, BLACK, "Legacy replacement:Y \t");
	else {
		printk(WHITE, BLACK, "Legacy replacement:N \t");
		while (1) IO_hlt(); 
	}
	// get min tick
	_minTick = (cReg >> 32) & ((1ul << 32) - 1);
	printk(WHITE, BLACK, " minTick=%d\n", _minTick);
	_setCfgQuad(_Cfg, 0x0);

	u64 cfg0 = _getTimerConfig(0);
	printk(YELLOW, BLACK, "HPET: Timer 0: cap&cfg:%#018lx \t", cfg0);
	if (cfg0 & _TimerCfgCap_64Cap) printk(WHITE, BLACK, "64-bit supply: Y \n");
	else printk(WHITE, BLACK, "64-bit supply:N \n");
	_setTimerConfig(0, _TimerCfgCap_Enable | _TimerCfgCap_Period | _TimerCfgCap_SetVal | _TimerCfgCap_Irq(2));
	// set it to 1 ms
	_setTimerComparator(0, (u64)(1 * 1e12 / _minTick + 1));

	_setCfgQuad(_MainCounterValue, 0x0);
	_setCfgQuad(_IntrStatus, 0xfffffffful);
	_setCfgQuad(_Cfg, 0x3);

	Intr_register(0x22, &_intrDesc, HW_Timer_HPET_handler, 0, &_intrCotroller, "HPET");
}

void HW_Timer_HPET_initAdvance() {
	printk(RED, BLACK, "HW_Timer_HPET_initAdvance()");
	// initialize the other comparators
	int cmpNum = 0;
	u64 cReg = _getCfgQuad(_CapId);
	cmpNum = (cReg >> 8) & 0xf;
	printk(WHITE, BLACK, "cmpNum:%d\n", cmpNum);
	if (cmpNum < 2) { printk(RED, BLACK, "No 2rd HPET comparator.\n"); return ; }
	// setup second comparator with no-period mode
	int cfg1 = _getTimerConfig(1);
	if (!(cfg1 & _TimerCfgCap_64Cap)) { printk(RED, BLACK, "HPET: Timer 1: 64-bit supply: N\n"); return ; }

}