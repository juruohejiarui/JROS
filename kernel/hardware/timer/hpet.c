#include "hpet.h"
#include "../../includes/hardware.h"
#include "../../includes/log.h"
#include "../../includes/memory.h"
#include "../../includes/interrupt.h"
#include "../../includes/task.h"
#include "../../includes/smp.h"

static u32 _minTick = 0;

static IntrController _intrCotroller;
static IntrHandler _intrHandler;
static APICRteDescriptor _intrDesc;

static HPETDescriptor *_hpetDesc;
static Atomic _jiffies;

static __always_inline__ u64 _getTimerConfig(u32 id) { return *(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address + 0x100 + 0x20 * id));}

static inline void _setTimerConfig(u32 id, u64 config) {
	u64 readonlyPart = *(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x100 + 0x20 * id) & 0x8030;
	// focused to run in 32-bit mode
	*(u64 *)((u64)DMAS_phys2Virt(_hpetDesc->address.Address) + 0x100 + 0x20 * id) = config | readonlyPart;
	IO_mfence();
}
static __always_inline__ void _setTimerComparator(u32 id, u64 comparator) {
	*(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x108 + 0x20 * id) = comparator;
	IO_mfence();
}

static int _mode;

IntrHandlerDeclare(HW_Timer_HPET_handler) {
	// print the counter
	if (_mode & 1) {
		Atomic_inc(&_jiffies);
		if (Task_cfsStruct.flags) Intr_SoftIrq_Timer_updateState();
	} else {
		Task_updateAllProcessorState();
	}
	_mode ^= 1;
	// printk(BLACK, WHITE, "H");
}

void HW_Timer_HPET_init() {
	printk(RED, BLACK, "HW_Timer_HPET_init()\n");
	// initializ the data structure
	_jiffies.value = _mode = 0;
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
		_intrDesc.destDesc.logical.destination = HW_APIC_getAPICID();
		printk(WHITE, BLACK, "HPET: timer interrupt at processor with apicID=%x\n", HW_APIC_getAPICID());
	}

	// set the general configuration register
	u64 cReg = *(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x00);
	printk(YELLOW, BLACK, "HPET: Capability register: %#018lx \t", *(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x00));
	if (cReg & 0x2000) printk(WHITE, BLACK, "64-bit supply: Y \t");
	else printk(WHITE, BLACK, "64-bit supply: N\t");
	if (cReg & 0x8000) printk(WHITE, BLACK, "Legacy replacement: Y \t");
	else {
		printk(WHITE, BLACK, "Legacy replacement: N \t");
		while (1) IO_hlt(); 
	}
	// get min tick
	_minTick = (cReg >> 32) & ((1ul << 32) - 1);
	printk(WHITE, BLACK, " minTick=%d\n", _minTick);
	*(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x10) = 0x0;

	u64 cfg0 = _getTimerConfig(0);
	if (cfg0 & 0x20) printk(WHITE, BLACK, "HPET: timer 0 support 64 bit.\n");
	else printk(WHITE, BLACK, "HPET: timer 0 support 32 bit.\n");
	_setTimerConfig(0, 0x40000004c);
	// set it to 0.5 ms
	_setTimerComparator(0, (u32)(0.5 * 1e12 / _minTick + 1));
	
	*(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0xf0) = 0x0;
	IO_mfence();
	*(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x20) = 0xffffffff;
	IO_mfence();
	*(u64 *)(DMAS_phys2Virt(_hpetDesc->address.Address) + 0x10) = 0x3;
	IO_mfence();

	Intr_register(0x22, &_intrDesc, HW_Timer_HPET_handler, 0, &_intrCotroller, "HPET");
}

i64 HW_Timer_HPET_jiffies() { return _jiffies.value; }
