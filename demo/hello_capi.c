/*
 * **************************************************
 *             Hello CAPI
 * **************************************************
 * Uses memcpy AFU to copy the string 'Hello Capi' from
 * a source buffer to a destination buffer. The content
 * of dstbuffer are printed at the end. If successful
 * the code should print an output of 'Hello Capi'.
 * The code demostrates the use of libcxl apis.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <endian.h>
#include <err.h>

#include <libcxl.h>

#include "memcpy_afu.h"

/* afu work element queue */
struct memcpy_weq weq;

/* Per process slave context */
struct cxl_afu_h * afu_h;

/* Ask afu to perform a strcpy operation and wait for the operation to finish */
int afu_memcpy(char * dst, char *src, size_t size);

/* Ask afu to perform a strcpy operation and wait for an event from the afu to finish */
int afu_memcpy2(char * dst, char *src, size_t size);

/* Code entry point */
int main(int argc, char *argv[])
{
	/* Sets up per process problem state area */
	int setup_afu_slave_psa(struct cxl_afu_h * afu);

	struct cxl_adapter_h * adapter = NULL;
	struct cxl_afu_h * afu = NULL;
	char srcbuffer[1024] , dstbuffer[1024];
	int rc = 1;

	/* ************* Adapter Discovery Phase ********** */

	/* Lookup if we have an capi adapter */
	if ((adapter = cxl_adapter_next(NULL)) == NULL) {
		warnx("No capi adapter found !!!");
		goto out;
	} else {
		printf("INFO: Using CXL adapter %s\n", cxl_adapter_dev_name(adapter));
	}

	/* ************* Accelerator Discovery Phase ********** */
	/* Lookup if we have an afu configured */
	if ((afu = cxl_adapter_afu_next(adapter, NULL)) == NULL) {
		warnx("No afu on the adapter found !!!");
		goto out;
	} else {
		printf("INFO: Using AFU %s\n", cxl_afu_dev_name(afu));
	}

	/* ********************* Setup Phase ******************* */
	/* Setup the slave afu context */
	if (setup_afu_slave_psa(afu)) {
		perror("Unable to setup slave psa");
		goto out;
	}

	/* *************** Computation Phase ***************** */
	/* All set now perform memcpy using a poll loop */
	strcpy(srcbuffer, "Hello CAPI");
	printf("INFO: Copying string 'Hello CAPI' to dest buffer using poll loop\n");
	rc = afu_memcpy(dstbuffer, srcbuffer, strlen(srcbuffer) + 1);
	if (rc) {
		perror("Unable to perform memcpy");
		goto out;
	}
	/* compare the strings */
	rc = strcmp(srcbuffer, dstbuffer);
	assert (rc == 0);
	printf(">> %s\n", dstbuffer);

	/* Now perform memcpy using read event */
	dstbuffer[0] = 0;
	printf("INFO: Copying string 'Hello CAPI' to dest buffer using read-event wait\n");
	rc = afu_memcpy2(dstbuffer, srcbuffer, strlen(srcbuffer) + 1);
	if (rc) {
		perror("Unable to perform memcpy");
		goto out;
	}
	/* compare the strings */
	rc = strcmp(srcbuffer, dstbuffer);
	assert (rc == 0);
	printf(">> %s\n", dstbuffer);

out:
	/* ********** Deinitialization Phase ************ */
	if (afu_h)
		cxl_afu_free(afu_h);
	if (afu)
		cxl_afu_free(afu);
	if (adapter)
		cxl_adapter_free(adapter);

 	return rc;
}

/* Ask afu to perform a strcpy operation and wait for the operation to finish */
int afu_memcpy(char * dst, char *src, size_t size)
{
	struct memcpy_work_element memcpy_we, *queued_we;
	int ret = 0;

	/* Setup a work element in the queue */
	memcpy_we.cmd = MEMCPY_WE_CMD(0, MEMCPY_WE_CMD_COPY);
	memcpy_we.status = 0;
	memcpy_we.length = htobe16((uint16_t)size);
	memcpy_we.src = htobe64((uintptr_t)src);
	memcpy_we.dst = htobe64((uintptr_t)dst);

	queued_we = memcpy_add_we(&weq, memcpy_we);
	queued_we->cmd |= MEMCPY_WE_CMD_VALID;

	/* restart the slice */
	cxl_mmio_write64(afu_h, MEMCPY_PS_REG_PCTRL,
			 0x8000000000000000ULL);

	/* We poll the status of the work element */
	while(!(queued_we->status & MEMCPY_WE_STAT_COMPLETE))
		sleep(1);

	return ret;
}

/* Ask afu to perform a strcpy operation and wait for an event from the afu to finish */
int afu_memcpy2(char * dst, char *src, size_t size)
{
	struct memcpy_work_element memcpy_we, irq_we, *queued_we;
	struct cxl_event event;
	int ret = 0;

	/* Setup a work element in the queue */
	memcpy_we.cmd = MEMCPY_WE_CMD(0, MEMCPY_WE_CMD_COPY);
	memcpy_we.status = 0;
	memcpy_we.length = htobe16((uint16_t)size);
	memcpy_we.src = htobe64((uintptr_t)src);
	memcpy_we.dst = htobe64((uintptr_t)dst);

	/* Setup the interrupt work element */
	irq_we.cmd = MEMCPY_WE_CMD(1, MEMCPY_WE_CMD_IRQ);
	irq_we.status = 0;
	irq_we.length = htobe16(3); /* Interrupt notification when the job finishes */
	irq_we.src = 0;
	irq_we.dst = 0;

	/* Add the primary work to the queue */
	queued_we = memcpy_add_we(&weq, memcpy_we);
	queued_we->cmd |= MEMCPY_WE_CMD_VALID;

	/* Add the interrupt work element to the queue */
	queued_we = memcpy_add_we(&weq, irq_we);
	queued_we->cmd |= MEMCPY_WE_CMD_VALID;


	/* restart the slice */
	cxl_mmio_write64(afu_h, MEMCPY_PS_REG_PCTRL,
			 0x8000000000000000ULL);

	/* Wait for an event from the afu */
	ret = cxl_read_expected_event(afu_h, &event, CXL_EVENT_AFU_INTERRUPT, 3);
	return ret;
}


#define CACHELINESIZE	128
#define QUEUE_SIZE	2
/*
 * Sets up the afu per process context. This allocates the work 
 * element queue and validates if the process element is valid.
 */
int setup_afu_slave_psa(struct cxl_afu_h * afu)
{

	int ret;
	__u64 wed,process_handle_memcpy, process_handle_ioctl;;

	/* Get handle to Per Process PSA context */
	afu_h = cxl_afu_open_h(afu, CXL_VIEW_SLAVE);
	if (afu_h == NULL) {
		perror("Unable to open AFU Slave cxl device");
		return 1;
	}

	/* initialize the work queue */
	memcpy_init_weq(&weq, QUEUE_SIZE * CACHELINESIZE);

	/* Point the work element descriptor (wed) at the weq */
	wed = MEMCPY_WED(weq.queue, QUEUE_SIZE);

	/* attach the wed to the context */
	ret = cxl_afu_attach(afu_h, wed);
	if (ret) {
		perror("Unable to attach the slave cxl context");
		return 1;
	}

	/* Map the per process psa to an application vma */
	ret = cxl_mmio_map(afu_h, CXL_MMIO_BIG_ENDIAN);
	if (cxl_mmio_map(afu_h, CXL_MMIO_BIG_ENDIAN) == -1) {
		perror("Unable to map problem state registers");
		return 1;
	}

	/* Fetch the process element from kernel */
 	process_handle_ioctl = cxl_afu_get_process_element(afu_h);
	if (process_handle_ioctl < 0) {
		perror("process_handle_ioctl");
		return 1;
	}

	/* read the process element handle from the PPSA */
	if (cxl_mmio_read64(afu_h, MEMCPY_PS_REG_PH, &process_handle_memcpy) == -1) {
		perror("Unable to read mmaped space");
		return 1;
	} else {
		process_handle_memcpy = process_handle_memcpy >> 48;
	}

	/* See if the process element is valid */
	if ((process_handle_memcpy == 0xdead) ||
	    (process_handle_memcpy == 0xffff))  {
		warnx("Bad process handle");
		return 1;
	}
	assert(process_handle_memcpy == process_handle_ioctl);

	return 0;
}

