#ifndef _MEMCPY_AFU_H_
#define _MEMCPY_AFU_H_

#include <linux/types.h>
#include <assert.h>

struct memcpy_weq {
	struct memcpy_work_element *queue;
	struct memcpy_work_element *next;
	struct memcpy_work_element *last;
	int wrap;
	int count;
};

struct memcpy_work_element {
	volatile __u8 cmd; /* valid, wrap, cmd */
	volatile __u8 status;
	__be16 length; /* also irq src */
	__be32 reserved1;
	__be64 reserved2;
	__be64 src;
	__be64 dst;
};
#define MEMCPY_WE_CMD(valid, cmd) \
	((((valid) & 0x1) << 7) |	\
	 (((cmd) & 0x3f) << 0))
#define MEMCPY_WE_CMD_COPY	0
#define MEMCPY_WE_CMD_IRQ	1
#define MEMCPY_WE_STAT_COMPLETE		0x80
#define MEMCPY_WE_STAT_TRANS_FAULT	0x40
#define MEMCPY_WE_STAT_AERROR		0x20
#define MEMCPY_WE_STAT_DERROR		0x10

#define MEMCPY_WE_CMD_VALID (0x1 << 7)
#define MEMCPY_WE_CMD_WRAP (0x1 << 6)

#define MEMCPY_WED(queue, depth)  \
	((((uintptr_t)queue) & 0xfffffffffffff000ULL) | \
	 (((__u64)depth) & 0xfffULL))

/* AFU PSA registers */
#define MEMCPY_AFU_PSA_REGS_SIZE 16*1024
#define MEMCPY_AFU_PSA_REG_CTL		0
#define MEMCPY_AFU_PSA_REG_ERR		2
#define MEMCPY_AFU_PSA_REG_ERR_INFO	3
#define MEMCPY_AFU_PSA_REG_TRACE_CTL	4

/* Per-process application registers */
#define MEMCPY_PS_REG_WED	(0 << 3)
#define MEMCPY_PS_REG_PH	(1 << 3)
#define MEMCPY_PS_REG_STATUS	(2 << 3)
#define MEMCPY_PS_REG_PCTRL	(3 << 3)
#define MEMCPY_PS_REG_TB	(8 << 3)

#define mb()   __asm__ __volatile__ ("sync" : : : "memory")

static inline int memcpy_queue_length(size_t queue_size)
{
	return queue_size/sizeof(struct memcpy_work_element);
}

void memcpy_init_weq(struct memcpy_weq *weq, size_t queue_size);
struct memcpy_work_element *memcpy_add_we(struct memcpy_weq *weq, struct memcpy_work_element we);

#endif /* _MEMCPY_AFU_H_ */
