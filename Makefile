#*********************************************************************************************************
# libsylixos Makefile
# target -> libsylixos.a
#*********************************************************************************************************

#*********************************************************************************************************
# configure area you can set the following config to you own system
# FPUFLAGS (-mfloat-abi=softfp -mfpu=vfpv3 ...)
# CPUFLAGS (-mcpu=arm920t ...)
# NOTICE: libsylixos, BSP and other kernel modules projects CAN NOT use vfp!
#*********************************************************************************************************
FPUFLAGS = -mfloat-abi=softfp -mfpu=vfpv3
CPUFLAGS = -mcpu=arm920t

#*********************************************************************************************************
# toolchain select
#*********************************************************************************************************
TOOLCHAIN_PROBE = $(shell arm-sylixos-eabi-gcc -v 2>null && \
					 (rm -rf null; echo commercial) || \
					 (rm -rf null; echo opensource))

ifneq (,$(findstring commercial, $(TOOLCHAIN_PROBE)))
TOOLCHAIN_PREFIX = arm-sylixos-eabi-
else 
TOOLCHAIN_PREFIX = arm-none-eabi-
endif

CC  = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
AS  = $(TOOLCHAIN_PREFIX)gcc
AR  = $(TOOLCHAIN_PREFIX)ar
LD  = $(TOOLCHAIN_PREFIX)g++

#*********************************************************************************************************
# debug options (debug or release)
#*********************************************************************************************************
DEBUG_LEVEL = debug

#*********************************************************************************************************
# build options
# do you want build process support library
#*********************************************************************************************************
BUILD_PROCESS_SUP_LIB = 1

#*********************************************************************************************************
# do you want build some usefull kernel module
#*********************************************************************************************************
BUILD_KERNEL_MODULE = 1

#*********************************************************************************************************
# do not change the following code
# buildin internal application source
#*********************************************************************************************************
APPL_SRCS = \
SylixOS/appl/editors/vi/vi_fix.c \
SylixOS/appl/editors/vi/vi_sylixos.c \
SylixOS/appl/editors/vi/src/vi.c \
SylixOS/appl/ini/iniparser/dictionary.c \
SylixOS/appl/ini/iniparser/iniparser.c \
SylixOS/appl/ssl/polarssl/library/aes.c \
SylixOS/appl/ssl/polarssl/library/arc4.c \
SylixOS/appl/ssl/polarssl/library/asn1parse.c \
SylixOS/appl/ssl/polarssl/library/asn1write.c \
SylixOS/appl/ssl/polarssl/library/base64.c \
SylixOS/appl/ssl/polarssl/library/bignum.c \
SylixOS/appl/ssl/polarssl/library/blowfish.c \
SylixOS/appl/ssl/polarssl/library/camellia.c \
SylixOS/appl/ssl/polarssl/library/certs.c \
SylixOS/appl/ssl/polarssl/library/cipher.c \
SylixOS/appl/ssl/polarssl/library/cipher_wrap.c \
SylixOS/appl/ssl/polarssl/library/ctr_drbg.c \
SylixOS/appl/ssl/polarssl/library/debug.c \
SylixOS/appl/ssl/polarssl/library/des.c \
SylixOS/appl/ssl/polarssl/library/dhm.c \
SylixOS/appl/ssl/polarssl/library/entropy.c \
SylixOS/appl/ssl/polarssl/library/entropy_poll.c \
SylixOS/appl/ssl/polarssl/library/error.c \
SylixOS/appl/ssl/polarssl/library/gcm.c \
SylixOS/appl/ssl/polarssl/library/havege.c \
SylixOS/appl/ssl/polarssl/library/md.c \
SylixOS/appl/ssl/polarssl/library/md2.c \
SylixOS/appl/ssl/polarssl/library/md4.c \
SylixOS/appl/ssl/polarssl/library/md5.c \
SylixOS/appl/ssl/polarssl/library/md_wrap.c \
SylixOS/appl/ssl/polarssl/library/net.c \
SylixOS/appl/ssl/polarssl/library/padlock.c \
SylixOS/appl/ssl/polarssl/library/pbkdf2.c \
SylixOS/appl/ssl/polarssl/library/pem.c \
SylixOS/appl/ssl/polarssl/library/pkcs11.c \
SylixOS/appl/ssl/polarssl/library/pkcs12.c \
SylixOS/appl/ssl/polarssl/library/pkcs5.c \
SylixOS/appl/ssl/polarssl/library/rsa.c \
SylixOS/appl/ssl/polarssl/library/sha1.c \
SylixOS/appl/ssl/polarssl/library/sha2.c \
SylixOS/appl/ssl/polarssl/library/sha4.c \
SylixOS/appl/ssl/polarssl/library/ssl_cache.c \
SylixOS/appl/ssl/polarssl/library/ssl_cli.c \
SylixOS/appl/ssl/polarssl/library/ssl_srv.c \
SylixOS/appl/ssl/polarssl/library/ssl_tls.c \
SylixOS/appl/ssl/polarssl/library/timing.c \
SylixOS/appl/ssl/polarssl/library/version.c \
SylixOS/appl/ssl/polarssl/library/x509parse.c \
SylixOS/appl/ssl/polarssl/library/x509write.c \
SylixOS/appl/ssl/polarssl/library/xtea.c \
SylixOS/appl/zip/zlib/zlib_sylixos.c \
SylixOS/appl/zip/zlib/src/adler32.c \
SylixOS/appl/zip/zlib/src/compress.c \
SylixOS/appl/zip/zlib/src/crc32.c \
SylixOS/appl/zip/zlib/src/deflate.c \
SylixOS/appl/zip/zlib/src/example.c \
SylixOS/appl/zip/zlib/src/gzclose.c \
SylixOS/appl/zip/zlib/src/gzlib.c \
SylixOS/appl/zip/zlib/src/gzread.c \
SylixOS/appl/zip/zlib/src/gzwrite.c \
SylixOS/appl/zip/zlib/src/infback.c \
SylixOS/appl/zip/zlib/src/inffast.c \
SylixOS/appl/zip/zlib/src/inflate.c \
SylixOS/appl/zip/zlib/src/inftrees.c \
SylixOS/appl/zip/zlib/src/minigzip.c \
SylixOS/appl/zip/zlib/src/trees.c \
SylixOS/appl/zip/zlib/src/uncompr.c \
SylixOS/appl/zip/zlib/src/zutil.c

#*********************************************************************************************************
# arch source
#*********************************************************************************************************
ARCH_SRCS = \
SylixOS/arch/arm/backtrace/armBacktrace.c \
SylixOS/arch/arm/common/cp15/armCp15Asm.S \
SylixOS/arch/arm/common/armAssert.c \
SylixOS/arch/arm/common/armContext.c \
SylixOS/arch/arm/common/armContextAsm.S \
SylixOS/arch/arm/common/armExc.c \
SylixOS/arch/arm/common/armExcAsm.S \
SylixOS/arch/arm/common/armLib.c \
SylixOS/arch/arm/common/armLibAsm.S \
SylixOS/arch/arm/dbg/armDbg.c \
SylixOS/arch/arm/dbg/armGdb.c \
SylixOS/arch/arm/dma/pl330/armPl330.c \
SylixOS/arch/arm/elf/armElf.c \
SylixOS/arch/arm/elf/armUnwind.c \
SylixOS/arch/arm/fpu/vfp9/armVfp9.c \
SylixOS/arch/arm/fpu/vfp9/armVfp9Asm.S \
SylixOS/arch/arm/fpu/vfp11/armVfp11.c \
SylixOS/arch/arm/fpu/vfp11/armVfp11Asm.S \
SylixOS/arch/arm/fpu/vfpv3/armVfpV3.c \
SylixOS/arch/arm/fpu/vfpv3/armVfpV3Asm.S \
SylixOS/arch/arm/fpu/vfpv4/armVfpV4.c \
SylixOS/arch/arm/fpu/vfpnone/armVfpNone.c \
SylixOS/arch/arm/fpu/armFpu.c \
SylixOS/arch/arm/mm/cache/l2/armL2.c \
SylixOS/arch/arm/mm/cache/l2/armL2A8.c \
SylixOS/arch/arm/mm/cache/l2/armL2x0.c \
SylixOS/arch/arm/mm/cache/v4/armCacheV4.c \
SylixOS/arch/arm/mm/cache/v4/armCacheV4Asm.S \
SylixOS/arch/arm/mm/cache/v5/armCacheV5.c \
SylixOS/arch/arm/mm/cache/v5/armCacheV5Asm.S \
SylixOS/arch/arm/mm/cache/v6/armCacheV6.c \
SylixOS/arch/arm/mm/cache/v6/armCacheV6Asm.S \
SylixOS/arch/arm/mm/cache/v7/armCacheV7.c \
SylixOS/arch/arm/mm/cache/v7/armCacheV7Asm.S \
SylixOS/arch/arm/mm/cache/armCacheCommonAsm.S \
SylixOS/arch/arm/mm/mmu/v4/armMmuV4.c \
SylixOS/arch/arm/mm/mmu/v7/armMmuV7.c \
SylixOS/arch/arm/mm/mmu/v7/armMmuV7Asm.S \
SylixOS/arch/arm/mm/mmu/armMmuCommon.c \
SylixOS/arch/arm/mm/mmu/armMmuCommonAsm.S \
SylixOS/arch/arm/mm/armCache.c \
SylixOS/arch/arm/mm/armMmu.c \
SylixOS/arch/arm/mpcore/scu/armScu.c \
SylixOS/arch/arm/mpcore/armMpCoreAsm.S \
SylixOS/arch/arm/mpcore/armSpinlock.c

#*********************************************************************************************************
# debug source
#*********************************************************************************************************
DEBUG_SRCS = \
SylixOS/debug/dtrace/dtrace.c \
SylixOS/debug/gdb/gdbserver.c

#*********************************************************************************************************
# file system source
#*********************************************************************************************************
FS_SRCS = \
SylixOS/fs/diskCache/diskCache.c \
SylixOS/fs/diskCache/diskCacheLib.c \
SylixOS/fs/diskCache/diskCacheProc.c \
SylixOS/fs/diskCache/diskCacheThread.c \
SylixOS/fs/diskPartition/diskPartition.c \
SylixOS/fs/diskRaid/diskRaid0.c \
SylixOS/fs/diskRaid/diskRaid1.c \
SylixOS/fs/diskRaid/diskRaidLib.c \
SylixOS/fs/fatFs/diskio.c \
SylixOS/fs/fatFs/fatFs.c \
SylixOS/fs/fatFs/fatstat.c \
SylixOS/fs/fatFs/fattime.c \
SylixOS/fs/fatFs/ff.c \
SylixOS/fs/fatFs/option/unicode.c \
SylixOS/fs/fatFs/option/syscall.c \
SylixOS/fs/fsCommon/fsCommon.c \
SylixOS/fs/mount/mount.c \
SylixOS/fs/mtd/mtdcore.c \
SylixOS/fs/mtd/linux/bch.c \
SylixOS/fs/mtd/linux/strim.c \
SylixOS/fs/mtd/nand/nand_base.c \
SylixOS/fs/mtd/nand/nand_bbt.c \
SylixOS/fs/mtd/nand/nand_bch.c \
SylixOS/fs/mtd/nand/nand_ecc.c \
SylixOS/fs/mtd/nand/nand_ids.c \
SylixOS/fs/mtd/onenand/onenand_base.c \
SylixOS/fs/mtd/onenand/onenand_bbt.c \
SylixOS/fs/nandRCache/nandRCache.c \
SylixOS/fs/nfs/mount_clnt.c \
SylixOS/fs/nfs/mount_xdr.c \
SylixOS/fs/nfs/nfs_clnt.c \
SylixOS/fs/nfs/nfs_sylixos.c \
SylixOS/fs/nfs/nfs_xdr.c \
SylixOS/fs/oemDisk/oemDisk.c \
SylixOS/fs/procFs/procFs.c \
SylixOS/fs/procFs/procFsLib.c \
SylixOS/fs/procFs/procBsp/procBsp.c \
SylixOS/fs/procFs/procFssup/procFssup.c \
SylixOS/fs/procFs/procKernel/procKernel.c \
SylixOS/fs/procFs/procPower/procPower.c \
SylixOS/fs/ramFs/ramFs.c \
SylixOS/fs/ramFs/ramFsLib.c \
SylixOS/fs/romFs/romFs.c \
SylixOS/fs/romFs/romFsLib.c \
SylixOS/fs/rootFs/rootFs.c \
SylixOS/fs/rootFs/rootFsLib.c \
SylixOS/fs/unique/unique.c \
SylixOS/fs/yaffs2/yaffs_allocator.c \
SylixOS/fs/yaffs2/yaffs_attribs.c \
SylixOS/fs/yaffs2/yaffs_bitmap.c \
SylixOS/fs/yaffs2/yaffs_checkptrw.c \
SylixOS/fs/yaffs2/yaffs_ecc.c \
SylixOS/fs/yaffs2/yaffs_guts.c \
SylixOS/fs/yaffs2/yaffs_mtdif_multi.c \
SylixOS/fs/yaffs2/yaffs_nameval.c \
SylixOS/fs/yaffs2/yaffs_nand.c \
SylixOS/fs/yaffs2/yaffs_packedtags1.c \
SylixOS/fs/yaffs2/yaffs_packedtags2.c \
SylixOS/fs/yaffs2/yaffs_summary.c \
SylixOS/fs/yaffs2/yaffs_sylixos.c \
SylixOS/fs/yaffs2/yaffs_sylixosproc.c \
SylixOS/fs/yaffs2/yaffs_tagscompat.c \
SylixOS/fs/yaffs2/yaffs_tagsmarshall.c \
SylixOS/fs/yaffs2/yaffs_verify.c \
SylixOS/fs/yaffs2/yaffs_yaffs1.c \
SylixOS/fs/yaffs2/yaffs_yaffs2.c \
SylixOS/fs/yaffs2/direct/yaffscfg.c \
SylixOS/fs/yaffs2/direct/yaffsfs.c \
SylixOS/fs/yaffs2/direct/yaffs_hweight.c \
SylixOS/fs/yaffs2/direct/yaffs_qsort.c

#*********************************************************************************************************
# gui tools source
#*********************************************************************************************************
GUI_SRCS = \
SylixOS/gui/input_device/input_device.c

#*********************************************************************************************************
# kernel source
#*********************************************************************************************************
KERN_SRCS = \
SylixOS/kernel/cache/cache.c \
SylixOS/kernel/core/_BitmapLib.c \
SylixOS/kernel/core/_CandTable.c \
SylixOS/kernel/core/_CoroutineLib.c \
SylixOS/kernel/core/_CoroutineShell.c \
SylixOS/kernel/core/_CpuLib.c \
SylixOS/kernel/core/_ErrorLib.c \
SylixOS/kernel/core/_EventHighLevel.c \
SylixOS/kernel/core/_EventInit.c \
SylixOS/kernel/core/_EventQueue.c \
SylixOS/kernel/core/_EventSetBlock.c \
SylixOS/kernel/core/_EventSetInit.c \
SylixOS/kernel/core/_EventSetReady.c \
SylixOS/kernel/core/_EventSetUnlink.c \
SylixOS/kernel/core/_GlobalInit.c \
SylixOS/kernel/core/_HeapInit.c \
SylixOS/kernel/core/_HeapLib.c \
SylixOS/kernel/core/_IdleThread.c \
SylixOS/kernel/core/_InterVectInit.c \
SylixOS/kernel/core/_ITimerThread.c \
SylixOS/kernel/core/_JobQueue.c \
SylixOS/kernel/core/_KernelHighLevelInit.c \
SylixOS/kernel/core/_KernelLowLevelInit.c \
SylixOS/kernel/core/_KernelStatus.c \
SylixOS/kernel/core/_MsgQueueInit.c \
SylixOS/kernel/core/_MsgQueueLib.c \
SylixOS/kernel/core/_Object.c \
SylixOS/kernel/core/_PartitionInit.c \
SylixOS/kernel/core/_PartitionLib.c \
SylixOS/kernel/core/_PriorityInit.c \
SylixOS/kernel/core/_ReadyRing.c \
SylixOS/kernel/core/_ReadyTableInit.c \
SylixOS/kernel/core/_ReadyTableLib.c \
SylixOS/kernel/core/_RmsInit.c \
SylixOS/kernel/core/_RmsLib.c \
SylixOS/kernel/core/_RtcInit.c \
SylixOS/kernel/core/_Sched.c \
SylixOS/kernel/core/_SchedCand.c \
SylixOS/kernel/core/_SmpIpi.c \
SylixOS/kernel/core/_SmpSpinlock.c \
SylixOS/kernel/core/_StackCheckInit.c \
SylixOS/kernel/core/_ThreadAffinity.c \
SylixOS/kernel/core/_ThreadFpu.c \
SylixOS/kernel/core/_ThreadIdInit.c \
SylixOS/kernel/core/_ThreadInit.c \
SylixOS/kernel/core/_ThreadJoinLib.c \
SylixOS/kernel/core/_ThreadMiscLib.c \
SylixOS/kernel/core/_ThreadSafeLib.c \
SylixOS/kernel/core/_ThreadShell.c \
SylixOS/kernel/core/_ThreadStatus.c \
SylixOS/kernel/core/_ThreadVarInit.c \
SylixOS/kernel/core/_ThreadVarLib.c \
SylixOS/kernel/core/_TimerInit.c \
SylixOS/kernel/core/_TimeTick.c \
SylixOS/kernel/core/_UpSpinlock.c \
SylixOS/kernel/core/_WakeupLine.c \
SylixOS/kernel/interface/CoroutineCreate.c \
SylixOS/kernel/interface/CoroutineDelete.c \
SylixOS/kernel/interface/CoroutineResume.c \
SylixOS/kernel/interface/CoroutineStackCheck.c \
SylixOS/kernel/interface/CoroutineYield.c \
SylixOS/kernel/interface/CpuActive.c \
SylixOS/kernel/interface/CpuPower.c \
SylixOS/kernel/interface/EventSetCreate.c \
SylixOS/kernel/interface/EventSetDelete.c \
SylixOS/kernel/interface/EventSetGet.c \
SylixOS/kernel/interface/EventSetGetName.c \
SylixOS/kernel/interface/EventSetSet.c \
SylixOS/kernel/interface/EventSetStatus.c \
SylixOS/kernel/interface/EventSetTryGet.c \
SylixOS/kernel/interface/GetLastError.c \
SylixOS/kernel/interface/InterContext.c \
SylixOS/kernel/interface/InterEnterExit.c \
SylixOS/kernel/interface/InterLock.c \
SylixOS/kernel/interface/InterStack.c \
SylixOS/kernel/interface/InterVectorBase.c \
SylixOS/kernel/interface/InterVectorConnect.c \
SylixOS/kernel/interface/InterVectorEnable.c \
SylixOS/kernel/interface/InterVectorFlag.c \
SylixOS/kernel/interface/InterVectorIsr.c \
SylixOS/kernel/interface/KernelAtomic.c \
SylixOS/kernel/interface/KernelFpu.c \
SylixOS/kernel/interface/KernelGetKid.c \
SylixOS/kernel/interface/KernelGetPriority.c \
SylixOS/kernel/interface/KernelGetThreadNum.c \
SylixOS/kernel/interface/KernelHeapInfo.c \
SylixOS/kernel/interface/KernelHook.c \
SylixOS/kernel/interface/KernelHookDelete.c \
SylixOS/kernel/interface/KernelIpi.c \
SylixOS/kernel/interface/KernelIsRunning.c \
SylixOS/kernel/interface/KernelMisc.c \
SylixOS/kernel/interface/KernelObject.c \
SylixOS/kernel/interface/KernelParam.c \
SylixOS/kernel/interface/KernelReboot.c \
SylixOS/kernel/interface/KernelResume.c \
SylixOS/kernel/interface/KernelSpinlock.c \
SylixOS/kernel/interface/KernelStart.c \
SylixOS/kernel/interface/KernelSuspend.c \
SylixOS/kernel/interface/KernelTicks.c \
SylixOS/kernel/interface/KernelUuid.c \
SylixOS/kernel/interface/KernelVersion.c \
SylixOS/kernel/interface/MsgQueueClear.c \
SylixOS/kernel/interface/MsgQueueCreate.c \
SylixOS/kernel/interface/MsgQueueDelete.c \
SylixOS/kernel/interface/MsgQueueFlush.c \
SylixOS/kernel/interface/MsgQueueGetName.c \
SylixOS/kernel/interface/MsgQueueReceive.c \
SylixOS/kernel/interface/MsgQueueSend.c \
SylixOS/kernel/interface/MsgQueueSendEx.c \
SylixOS/kernel/interface/MsgQueueStatus.c \
SylixOS/kernel/interface/MsgQueueStatusEx.c \
SylixOS/kernel/interface/MsgQueueTryReceive.c \
SylixOS/kernel/interface/PartitionCreate.c \
SylixOS/kernel/interface/PartitionDelete.c \
SylixOS/kernel/interface/PartitionGet.c \
SylixOS/kernel/interface/PartitionGetName.c \
SylixOS/kernel/interface/PartitionPut.c \
SylixOS/kernel/interface/PartitionStatus.c \
SylixOS/kernel/interface/RegionAddMem.c \
SylixOS/kernel/interface/RegionCreate.c \
SylixOS/kernel/interface/RegionDelete.c \
SylixOS/kernel/interface/RegionGet.c \
SylixOS/kernel/interface/RegionGetAlign.c \
SylixOS/kernel/interface/RegionGetName.c \
SylixOS/kernel/interface/RegionPut.c \
SylixOS/kernel/interface/RegionReget.c \
SylixOS/kernel/interface/RegionStatus.c \
SylixOS/kernel/interface/RegionStatusEx.c \
SylixOS/kernel/interface/RmsCancel.c \
SylixOS/kernel/interface/RmsCreate.c \
SylixOS/kernel/interface/RmsDelete.c \
SylixOS/kernel/interface/RmsExecTimeGet.c \
SylixOS/kernel/interface/RmsGetName.c \
SylixOS/kernel/interface/RmsPeriod.c \
SylixOS/kernel/interface/RmsStatus.c \
SylixOS/kernel/interface/SemaphoreBClear.c \
SylixOS/kernel/interface/SemaphoreBCreate.c \
SylixOS/kernel/interface/SemaphoreBDelete.c \
SylixOS/kernel/interface/SemaphoreBFlush.c \
SylixOS/kernel/interface/SemaphoreBPend.c \
SylixOS/kernel/interface/SemaphoreBPendEx.c \
SylixOS/kernel/interface/SemaphoreBPost.c \
SylixOS/kernel/interface/SemaphoreBPostEx.c \
SylixOS/kernel/interface/SemaphoreBRelease.c \
SylixOS/kernel/interface/SemaphoreBStatus.c \
SylixOS/kernel/interface/SemaphoreBTryPend.c \
SylixOS/kernel/interface/SemaphoreCClear.c \
SylixOS/kernel/interface/SemaphoreCCreate.c \
SylixOS/kernel/interface/SemaphoreCDelete.c \
SylixOS/kernel/interface/SemaphoreCFlush.c \
SylixOS/kernel/interface/SemaphoreCPend.c \
SylixOS/kernel/interface/SemaphoreCPost.c \
SylixOS/kernel/interface/SemaphoreCRelease.c \
SylixOS/kernel/interface/SemaphoreCStatus.c \
SylixOS/kernel/interface/SemaphoreCStatusEx.c \
SylixOS/kernel/interface/SemaphoreCTryPend.c \
SylixOS/kernel/interface/SemaphoreDelete.c \
SylixOS/kernel/interface/SemaphoreFlush.c \
SylixOS/kernel/interface/SemaphoreGetName.c \
SylixOS/kernel/interface/SemaphoreMCreate.c \
SylixOS/kernel/interface/SemaphoreMDelete.c \
SylixOS/kernel/interface/SemaphoreMPend.c \
SylixOS/kernel/interface/SemaphoreMPost.c \
SylixOS/kernel/interface/SemaphoreMStatus.c \
SylixOS/kernel/interface/SemaphoreMStatusEx.c \
SylixOS/kernel/interface/SemaphorePend.c \
SylixOS/kernel/interface/SemaphorePost.c \
SylixOS/kernel/interface/SemaphorePostPend.c \
SylixOS/kernel/interface/ThreadAffinity.c \
SylixOS/kernel/interface/ThreadAttrBuild.c \
SylixOS/kernel/interface/ThreadCancel.c \
SylixOS/kernel/interface/ThreadCancelWatchDog.c \
SylixOS/kernel/interface/ThreadCPUUsageRefresh.c \
SylixOS/kernel/interface/ThreadCreate.c \
SylixOS/kernel/interface/ThreadDelete.c \
SylixOS/kernel/interface/ThreadDesc.c \
SylixOS/kernel/interface/ThreadDetach.c \
SylixOS/kernel/interface/ThreadFeedWatchDog.c \
SylixOS/kernel/interface/ThreadForceDelete.c \
SylixOS/kernel/interface/ThreadForceResume.c \
SylixOS/kernel/interface/ThreadGetCPUUsage.c \
SylixOS/kernel/interface/ThreadGetName.c \
SylixOS/kernel/interface/ThreadGetNotePad.c \
SylixOS/kernel/interface/ThreadGetPriority.c \
SylixOS/kernel/interface/ThreadGetSchedParam.c \
SylixOS/kernel/interface/ThreadGetSlice.c \
SylixOS/kernel/interface/ThreadGetStackMini.c \
SylixOS/kernel/interface/ThreadIdSelf.c \
SylixOS/kernel/interface/ThreadInit.c \
SylixOS/kernel/interface/ThreadIsLock.c \
SylixOS/kernel/interface/ThreadIsReady.c \
SylixOS/kernel/interface/ThreadIsSafe.c \
SylixOS/kernel/interface/ThreadIsSuspend.c \
SylixOS/kernel/interface/ThreadJoin.c \
SylixOS/kernel/interface/ThreadLock.c \
SylixOS/kernel/interface/ThreadRestart.c \
SylixOS/kernel/interface/ThreadResume.c \
SylixOS/kernel/interface/ThreadSafe.c \
SylixOS/kernel/interface/ThreadSetCancelState.c \
SylixOS/kernel/interface/ThreadSetCancelType.c \
SylixOS/kernel/interface/ThreadSetName.c \
SylixOS/kernel/interface/ThreadSetNotePad.c \
SylixOS/kernel/interface/ThreadSetPriority.c \
SylixOS/kernel/interface/ThreadSetSchedParam.c \
SylixOS/kernel/interface/ThreadSetSlice.c \
SylixOS/kernel/interface/ThreadStackCheck.c \
SylixOS/kernel/interface/ThreadStart.c \
SylixOS/kernel/interface/ThreadStop.c \
SylixOS/kernel/interface/ThreadSuspend.c \
SylixOS/kernel/interface/ThreadTestCancel.c \
SylixOS/kernel/interface/ThreadUnlock.c \
SylixOS/kernel/interface/ThreadUnsafe.c \
SylixOS/kernel/interface/ThreadVarAdd.c \
SylixOS/kernel/interface/ThreadVarDelete.c \
SylixOS/kernel/interface/ThreadVarGet.c \
SylixOS/kernel/interface/ThreadVarInfo.c \
SylixOS/kernel/interface/ThreadVarSet.c \
SylixOS/kernel/interface/ThreadVerify.c \
SylixOS/kernel/interface/ThreadWakeup.c \
SylixOS/kernel/interface/ThreadYield.c \
SylixOS/kernel/interface/TimeGet.c \
SylixOS/kernel/interface/TimerCancel.c \
SylixOS/kernel/interface/TimerCreate.c \
SylixOS/kernel/interface/TimerDelete.c \
SylixOS/kernel/interface/TimerGetName.c \
SylixOS/kernel/interface/TimerHGetFrequency.c \
SylixOS/kernel/interface/TimerHTicks.c \
SylixOS/kernel/interface/TimerReset.c \
SylixOS/kernel/interface/TimerStart.c \
SylixOS/kernel/interface/TimerStatus.c \
SylixOS/kernel/interface/TimeSleep.c \
SylixOS/kernel/interface/TimeTod.c \
SylixOS/kernel/interface/ugid.c \
SylixOS/kernel/list/listEvent.c \
SylixOS/kernel/list/listEventSet.c \
SylixOS/kernel/list/listHeap.c \
SylixOS/kernel/list/listLink.c \
SylixOS/kernel/list/listMsgQueue.c \
SylixOS/kernel/list/listPartition.c \
SylixOS/kernel/list/listRms.c \
SylixOS/kernel/list/listThread.c \
SylixOS/kernel/list/listThreadVar.c \
SylixOS/kernel/list/listTimer.c \
SylixOS/kernel/resource/resourceLib.c \
SylixOS/kernel/resource/resourceReclaim.c \
SylixOS/kernel/show/CPUUsageShow.c \
SylixOS/kernel/show/EventSetShow.c \
SylixOS/kernel/show/InterShow.c \
SylixOS/kernel/show/MsgQueueShow.c \
SylixOS/kernel/show/PartitionShow.c \
SylixOS/kernel/show/RegionShow.c \
SylixOS/kernel/show/RmsShow.c \
SylixOS/kernel/show/SemaphoreShow.c \
SylixOS/kernel/show/StackShow.c \
SylixOS/kernel/show/ThreadShow.c \
SylixOS/kernel/show/TimerShow.c \
SylixOS/kernel/show/TimeShow.c \
SylixOS/kernel/show/VmmShow.c \
SylixOS/kernel/threadext/ThreadCleanup.c \
SylixOS/kernel/threadext/ThreadCond.c \
SylixOS/kernel/threadext/ThreadOnce.c \
SylixOS/kernel/tree/treeRb.c \
SylixOS/kernel/vmm/pageLib.c \
SylixOS/kernel/vmm/pageTable.c \
SylixOS/kernel/vmm/phyPage.c \
SylixOS/kernel/vmm/virPage.c \
SylixOS/kernel/vmm/vmm.c \
SylixOS/kernel/vmm/vmmAbort.c \
SylixOS/kernel/vmm/vmmArea.c \
SylixOS/kernel/vmm/vmmMalloc.c \
SylixOS/kernel/vmm/vmmSwap.c

#*********************************************************************************************************
# buildin library source
#*********************************************************************************************************
LIB_SRCS = \
SylixOS/lib/extern/libc.c \
SylixOS/lib/libc/crypt/bcrypt.c \
SylixOS/lib/libc/crypt/crypt-sha1.c \
SylixOS/lib/libc/crypt/crypt.c \
SylixOS/lib/libc/crypt/hmac_sha1.c \
SylixOS/lib/libc/crypt/md5crypt.c \
SylixOS/lib/libc/crypt/pw_gensalt.c \
SylixOS/lib/libc/crypt/util.c \
SylixOS/lib/libc/ctype/lib_ctype.c \
SylixOS/lib/libc/error/lib_panic.c \
SylixOS/lib/libc/float/lib_isinf.c \
SylixOS/lib/libc/inttypes/lib_inttypes.c \
SylixOS/lib/libc/locale/lib_locale.c \
SylixOS/lib/libc/setjmp/setjmp.c \
SylixOS/lib/libc/stdio/asprintf.c \
SylixOS/lib/libc/stdio/clrerr.c \
SylixOS/lib/libc/stdio/ctermid.c \
SylixOS/lib/libc/stdio/fclose.c \
SylixOS/lib/libc/stdio/fdopen.c \
SylixOS/lib/libc/stdio/fdprintf.c \
SylixOS/lib/libc/stdio/fdscanf.c \
SylixOS/lib/libc/stdio/feof.c \
SylixOS/lib/libc/stdio/ferror.c \
SylixOS/lib/libc/stdio/fflush.c \
SylixOS/lib/libc/stdio/fgetc.c \
SylixOS/lib/libc/stdio/fgetln.c \
SylixOS/lib/libc/stdio/fgetpos.c \
SylixOS/lib/libc/stdio/fgets.c \
SylixOS/lib/libc/stdio/fileno.c \
SylixOS/lib/libc/stdio/findfp.c \
SylixOS/lib/libc/stdio/flags.c \
SylixOS/lib/libc/stdio/fopen.c \
SylixOS/lib/libc/stdio/fprintf.c \
SylixOS/lib/libc/stdio/fpurge.c \
SylixOS/lib/libc/stdio/fputc.c \
SylixOS/lib/libc/stdio/fputs.c \
SylixOS/lib/libc/stdio/fread.c \
SylixOS/lib/libc/stdio/freopen.c \
SylixOS/lib/libc/stdio/fscanf.c \
SylixOS/lib/libc/stdio/fseek.c \
SylixOS/lib/libc/stdio/fsetpos.c \
SylixOS/lib/libc/stdio/ftell.c \
SylixOS/lib/libc/stdio/funopen.c \
SylixOS/lib/libc/stdio/fvwrite.c \
SylixOS/lib/libc/stdio/fwalk.c \
SylixOS/lib/libc/stdio/fwrite.c \
SylixOS/lib/libc/stdio/getc.c \
SylixOS/lib/libc/stdio/getchar.c \
SylixOS/lib/libc/stdio/gets.c \
SylixOS/lib/libc/stdio/getw.c \
SylixOS/lib/libc/stdio/lib_file.c \
SylixOS/lib/libc/stdio/makebuf.c \
SylixOS/lib/libc/stdio/mktemp.c \
SylixOS/lib/libc/stdio/perror.c \
SylixOS/lib/libc/stdio/popen.c \
SylixOS/lib/libc/stdio/printf.c \
SylixOS/lib/libc/stdio/putc.c \
SylixOS/lib/libc/stdio/putchar.c \
SylixOS/lib/libc/stdio/puts.c \
SylixOS/lib/libc/stdio/putw.c \
SylixOS/lib/libc/stdio/refill.c \
SylixOS/lib/libc/stdio/remove.c \
SylixOS/lib/libc/stdio/rewind.c \
SylixOS/lib/libc/stdio/rget.c \
SylixOS/lib/libc/stdio/scanf.c \
SylixOS/lib/libc/stdio/setbuf.c \
SylixOS/lib/libc/stdio/setbuffer.c \
SylixOS/lib/libc/stdio/setvbuf.c \
SylixOS/lib/libc/stdio/snprintf.c \
SylixOS/lib/libc/stdio/sprintf.c \
SylixOS/lib/libc/stdio/sscanf.c \
SylixOS/lib/libc/stdio/stdio.c \
SylixOS/lib/libc/stdio/tempnam.c \
SylixOS/lib/libc/stdio/tmpfile.c \
SylixOS/lib/libc/stdio/tmpnam.c \
SylixOS/lib/libc/stdio/ungetc.c \
SylixOS/lib/libc/stdio/vfprintf.c \
SylixOS/lib/libc/stdio/vfscanf.c \
SylixOS/lib/libc/stdio/vprintf.c \
SylixOS/lib/libc/stdio/vscanf.c \
SylixOS/lib/libc/stdio/vsnprintf.c \
SylixOS/lib/libc/stdio/vsprintf.c \
SylixOS/lib/libc/stdio/vsscanf.c \
SylixOS/lib/libc/stdio/wbuf.c \
SylixOS/lib/libc/stdio/wsetup.c \
SylixOS/lib/libc/stdlib/lib_abs.c \
SylixOS/lib/libc/stdlib/lib_env.c \
SylixOS/lib/libc/stdlib/lib_lldiv.c \
SylixOS/lib/libc/stdlib/lib_memlib.c \
SylixOS/lib/libc/stdlib/lib_rand.c \
SylixOS/lib/libc/stdlib/lib_search.c \
SylixOS/lib/libc/stdlib/lib_sort.c \
SylixOS/lib/libc/stdlib/lib_strto.c \
SylixOS/lib/libc/stdlib/lib_strtod.c \
SylixOS/lib/libc/stdlib/lib_system.c \
SylixOS/lib/libc/string/lib_ffs.c \
SylixOS/lib/libc/string/lib_index.c \
SylixOS/lib/libc/string/lib_memchr.c \
SylixOS/lib/libc/string/lib_memcmp.c \
SylixOS/lib/libc/string/lib_memcpy.c \
SylixOS/lib/libc/string/lib_mempcpy.c \
SylixOS/lib/libc/string/lib_memrchr.c \
SylixOS/lib/libc/string/lib_memset.c \
SylixOS/lib/libc/string/lib_rindex.c \
SylixOS/lib/libc/string/lib_stpcpy.c \
SylixOS/lib/libc/string/lib_strcasecmp.c \
SylixOS/lib/libc/string/lib_strcat.c \
SylixOS/lib/libc/string/lib_strchrnul.c \
SylixOS/lib/libc/string/lib_strcmp.c \
SylixOS/lib/libc/string/lib_strcpy.c \
SylixOS/lib/libc/string/lib_strcspn.c \
SylixOS/lib/libc/string/lib_strdup.c \
SylixOS/lib/libc/string/lib_strerror.c \
SylixOS/lib/libc/string/lib_strftime.c \
SylixOS/lib/libc/string/lib_stricmp.c \
SylixOS/lib/libc/string/lib_strlen.c \
SylixOS/lib/libc/string/lib_strncasecmp.c \
SylixOS/lib/libc/string/lib_strncat.c \
SylixOS/lib/libc/string/lib_strncmp.c \
SylixOS/lib/libc/string/lib_strncpy.c \
SylixOS/lib/libc/string/lib_strndup.c \
SylixOS/lib/libc/string/lib_strnlen.c \
SylixOS/lib/libc/string/lib_strnset.c \
SylixOS/lib/libc/string/lib_strpbrk.c \
SylixOS/lib/libc/string/lib_strptime.c \
SylixOS/lib/libc/string/lib_strsignal.c \
SylixOS/lib/libc/string/lib_strspn.c \
SylixOS/lib/libc/string/lib_strstr.c \
SylixOS/lib/libc/string/lib_strtok.c \
SylixOS/lib/libc/string/lib_strxfrm.c \
SylixOS/lib/libc/string/lib_swab.c \
SylixOS/lib/libc/string/lib_tolower.c \
SylixOS/lib/libc/string/lib_toupper.c \
SylixOS/lib/libc/string/lib_xstrdup.c \
SylixOS/lib/libc/string/lib_xstrndup.c \
SylixOS/lib/libc/sys/lib_statvfs.c \
SylixOS/lib/libc/time/lib_asctime.c \
SylixOS/lib/libc/time/lib_clock.c \
SylixOS/lib/libc/time/lib_ctime.c \
SylixOS/lib/libc/time/lib_daytime.c \
SylixOS/lib/libc/time/lib_difftime.c \
SylixOS/lib/libc/time/lib_gmtime.c \
SylixOS/lib/libc/time/lib_hrtime.c \
SylixOS/lib/libc/time/lib_localtime.c \
SylixOS/lib/libc/time/lib_mktime.c \
SylixOS/lib/libc/time/lib_time.c \
SylixOS/lib/libc/time/lib_tzset.c \
SylixOS/lib/libc/user/getpwent.c \
SylixOS/lib/libc/user/getshadow.c \
SylixOS/lib/libc/wchar/wchar.c \
SylixOS/lib/libc/wchar/wcscasecmp.c \
SylixOS/lib/libc/wchar/wcscat.c \
SylixOS/lib/libc/wchar/wcschr.c \
SylixOS/lib/libc/wchar/wcscmp.c \
SylixOS/lib/libc/wchar/wcscpy.c \
SylixOS/lib/libc/wchar/wcscspn.c \
SylixOS/lib/libc/wchar/wcsdup.c \
SylixOS/lib/libc/wchar/wcslcat.c \
SylixOS/lib/libc/wchar/wcslcpy.c \
SylixOS/lib/libc/wchar/wcslen.c \
SylixOS/lib/libc/wchar/wcsncasecmp.c \
SylixOS/lib/libc/wchar/wcsncat.c \
SylixOS/lib/libc/wchar/wcsncmp.c \
SylixOS/lib/libc/wchar/wcsncpy.c \
SylixOS/lib/libc/wchar/wcspbrk.c \
SylixOS/lib/libc/wchar/wcsrchr.c \
SylixOS/lib/libc/wchar/wcsspn.c \
SylixOS/lib/libc/wchar/wcsstr.c \
SylixOS/lib/libc/wchar/wcstok.c \
SylixOS/lib/libc/wchar/wcswcs.c \
SylixOS/lib/libc/wchar/wmemchr.c \
SylixOS/lib/libc/wchar/wmemcmp.c \
SylixOS/lib/libc/wchar/wmemcpy.c \
SylixOS/lib/libc/wchar/wmemmove.c \
SylixOS/lib/libc/wchar/wmemset.c \
SylixOS/lib/nl_compatible/nl_reent.c

#*********************************************************************************************************
# loader source
#*********************************************************************************************************
LOADER_SRCS = \
SylixOS/loader/elf/elf_loader.c \
SylixOS/loader/src/loader.c \
SylixOS/loader/src/loader_affinity.c \
SylixOS/loader/src/loader_exec.c \
SylixOS/loader/src/loader_file.c \
SylixOS/loader/src/loader_malloc.c \
SylixOS/loader/src/loader_proc.c \
SylixOS/loader/src/loader_shell.c \
SylixOS/loader/src/loader_symbol.c \
SylixOS/loader/src/loader_vppatch.c \
SylixOS/loader/src/loader_vptimer.c \
SylixOS/loader/src/loader_wait.c

#*********************************************************************************************************
# monitor source
#*********************************************************************************************************
MONITOR_SRCS = \
SylixOS/monitor/src/monitorBuffer.c \
SylixOS/monitor/src/monitorFileUpload.c \
SylixOS/monitor/src/monitorNetUpload.c \
SylixOS/monitor/src/monitorTrace.c \
SylixOS/monitor/src/monitorUpload.c

#*********************************************************************************************************
# mpi source
#*********************************************************************************************************
MPI_SRCS = \
SylixOS/mpi/dualportmem/dualportmem.c \
SylixOS/mpi/dualportmem/dualportmemLib.c \
SylixOS/mpi/src/mpiInit.c

#*********************************************************************************************************
# net source
#*********************************************************************************************************
NET_SRCS = \
SylixOS/net/libc/gethostbyht.c \
SylixOS/net/libc/getproto.c \
SylixOS/net/libc/getprotoby.c \
SylixOS/net/libc/getprotoent.c \
SylixOS/net/libc/getprotoname.c \
SylixOS/net/libc/getservbyname.c \
SylixOS/net/libc/getservbyport.c \
SylixOS/net/libc/getservent.c \
SylixOS/net/libc/inet_ntop.c \
SylixOS/net/libc/inet_pton.c \
SylixOS/net/lwip/lwip_fix.c \
SylixOS/net/lwip/lwip_if.c \
SylixOS/net/lwip/lwip_jobqueue.c \
SylixOS/net/lwip/lwip_netifnum.c \
SylixOS/net/lwip/lwip_netstat.c \
SylixOS/net/lwip/lwip_privatemib.c \
SylixOS/net/lwip/lwip_route.c \
SylixOS/net/lwip/lwip_shell.c \
SylixOS/net/lwip/lwip_shell6.c \
SylixOS/net/lwip/lwip_socket.c \
SylixOS/net/lwip/lwip_sylixos.c \
SylixOS/net/lwip/event/lwip_netevent.c \
SylixOS/net/lwip/packet/af_packet_eth.c \
SylixOS/net/lwip/packet/af_packet.c \
SylixOS/net/lwip/proc/lwip_proc.c \
SylixOS/net/lwip/src/api/api_lib.c \
SylixOS/net/lwip/src/api/api_msg.c \
SylixOS/net/lwip/src/api/err.c \
SylixOS/net/lwip/src/api/netbuf.c \
SylixOS/net/lwip/src/api/netdb.c \
SylixOS/net/lwip/src/api/netifapi.c \
SylixOS/net/lwip/src/api/pppapi.c \
SylixOS/net/lwip/src/api/sockets.c \
SylixOS/net/lwip/src/api/tcpip.c \
SylixOS/net/lwip/src/core/def.c \
SylixOS/net/lwip/src/core/dhcp.c \
SylixOS/net/lwip/src/core/dns.c \
SylixOS/net/lwip/src/core/inet_chksum.c \
SylixOS/net/lwip/src/core/init.c \
SylixOS/net/lwip/src/core/mem.c \
SylixOS/net/lwip/src/core/memp.c \
SylixOS/net/lwip/src/core/netif.c \
SylixOS/net/lwip/src/core/pbuf.c \
SylixOS/net/lwip/src/core/raw.c \
SylixOS/net/lwip/src/core/stats.c \
SylixOS/net/lwip/src/core/sys.c \
SylixOS/net/lwip/src/core/tcp.c \
SylixOS/net/lwip/src/core/tcp_in.c \
SylixOS/net/lwip/src/core/tcp_out.c \
SylixOS/net/lwip/src/core/timers.c \
SylixOS/net/lwip/src/core/udp.c \
SylixOS/net/lwip/src/core/ipv4/autoip.c \
SylixOS/net/lwip/src/core/ipv4/icmp.c \
SylixOS/net/lwip/src/core/ipv4/igmp.c \
SylixOS/net/lwip/src/core/ipv4/ip4.c \
SylixOS/net/lwip/src/core/ipv4/ip4_addr.c \
SylixOS/net/lwip/src/core/ipv4/ip_frag.c \
SylixOS/net/lwip/src/core/ipv6/dhcp6.c \
SylixOS/net/lwip/src/core/ipv6/ethip6.c \
SylixOS/net/lwip/src/core/ipv6/icmp6.c \
SylixOS/net/lwip/src/core/ipv6/inet6.c \
SylixOS/net/lwip/src/core/ipv6/ip6.c \
SylixOS/net/lwip/src/core/ipv6/ip6_addr.c \
SylixOS/net/lwip/src/core/ipv6/ip6_frag.c \
SylixOS/net/lwip/src/core/ipv6/mld6.c \
SylixOS/net/lwip/src/core/ipv6/nd6.c \
SylixOS/net/lwip/src/core/snmp/asn1_dec.c \
SylixOS/net/lwip/src/core/snmp/asn1_enc.c \
SylixOS/net/lwip/src/core/snmp/mib2.c \
SylixOS/net/lwip/src/core/snmp/mib_structs.c \
SylixOS/net/lwip/src/core/snmp/msg_in.c \
SylixOS/net/lwip/src/core/snmp/msg_out.c \
SylixOS/net/lwip/src/netif/etharp.c \
SylixOS/net/lwip/src/netif/ethernetif.c \
SylixOS/net/lwip/src/netif/ifqueue.c \
SylixOS/net/lwip/src/netif/slipif.c \
SylixOS/net/lwip/src/netif/aodv/aodv_hello.c \
SylixOS/net/lwip/src/netif/aodv/aodv_if.c \
SylixOS/net/lwip/src/netif/aodv/aodv_mcast.c \
SylixOS/net/lwip/src/netif/aodv/aodv_mtunnel.c \
SylixOS/net/lwip/src/netif/aodv/aodv_neighbor.c \
SylixOS/net/lwip/src/netif/aodv/aodv_proto.c \
SylixOS/net/lwip/src/netif/aodv/aodv_rerr.c \
SylixOS/net/lwip/src/netif/aodv/aodv_route.c \
SylixOS/net/lwip/src/netif/aodv/aodv_rrep.c \
SylixOS/net/lwip/src/netif/aodv/aodv_rreq.c \
SylixOS/net/lwip/src/netif/aodv/aodv_seeklist.c \
SylixOS/net/lwip/src/netif/aodv/aodv_timer.c \
SylixOS/net/lwip/src/netif/aodv/aodv_timercb.c \
SylixOS/net/lwip/src/netif/ppp/auth.c \
SylixOS/net/lwip/src/netif/ppp/ccp.c \
SylixOS/net/lwip/src/netif/ppp/chap-md5.c \
SylixOS/net/lwip/src/netif/ppp/chap-new.c \
SylixOS/net/lwip/src/netif/ppp/chap_ms.c \
SylixOS/net/lwip/src/netif/ppp/demand.c \
SylixOS/net/lwip/src/netif/ppp/eap.c \
SylixOS/net/lwip/src/netif/ppp/ecp.c \
SylixOS/net/lwip/src/netif/ppp/eui64.c \
SylixOS/net/lwip/src/netif/ppp/fsm.c \
SylixOS/net/lwip/src/netif/ppp/ipcp.c \
SylixOS/net/lwip/src/netif/ppp/ipv6cp.c \
SylixOS/net/lwip/src/netif/ppp/lcp.c \
SylixOS/net/lwip/src/netif/ppp/magic.c \
SylixOS/net/lwip/src/netif/ppp/multilink.c \
SylixOS/net/lwip/src/netif/ppp/ppp.c \
SylixOS/net/lwip/src/netif/ppp/pppcrypt.c \
SylixOS/net/lwip/src/netif/ppp/pppoe.c \
SylixOS/net/lwip/src/netif/ppp/pppol2tp.c \
SylixOS/net/lwip/src/netif/ppp/pppos.c \
SylixOS/net/lwip/src/netif/ppp/upap.c \
SylixOS/net/lwip/src/netif/ppp/utils.c \
SylixOS/net/lwip/src/netif/ppp/vj.c \
SylixOS/net/lwip/src/netif/radio/aes_crypt.c \
SylixOS/net/lwip/src/netif/radio/crypt_driver.c \
SylixOS/net/lwip/src/netif/radio/csma_mac.c \
SylixOS/net/lwip/src/netif/radio/ieee802154_aes_ccm.c \
SylixOS/net/lwip/src/netif/radio/ieee802154_aes.c \
SylixOS/net/lwip/src/netif/radio/ieee802154_eth.c \
SylixOS/net/lwip/src/netif/radio/ieee802154_frame.c \
SylixOS/net/lwip/src/netif/radio/lowpan_compress.c \
SylixOS/net/lwip/src/netif/radio/lowpan_frag.c \
SylixOS/net/lwip/src/netif/radio/lowpan_if.c \
SylixOS/net/lwip/src/netif/radio/mac_driver.c \
SylixOS/net/lwip/src/netif/radio/null_mac.c \
SylixOS/net/lwip/src/netif/radio/null_rdc.c \
SylixOS/net/lwip/src/netif/radio/simple_crypt.c \
SylixOS/net/lwip/src/netif/radio/tdma_mac.c \
SylixOS/net/lwip/src/netif/radio/xmac_rdc.c \
SylixOS/net/lwip/tools/ftp/lwip_ftp.c \
SylixOS/net/lwip/tools/ftp/lwip_ftpd.c \
SylixOS/net/lwip/tools/hosttable/lwip_hosttable.c \
SylixOS/net/lwip/tools/iac/lwip_iac.c \
SylixOS/net/lwip/tools/nat/lwip_nat.c \
SylixOS/net/lwip/tools/nat/lwip_natlib.c \
SylixOS/net/lwip/tools/netbios/lwip_netbios.c \
SylixOS/net/lwip/tools/npf/lwip_npf.c \
SylixOS/net/lwip/tools/ping/lwip_ping.c \
SylixOS/net/lwip/tools/ping6/lwip_ping6.c \
SylixOS/net/lwip/tools/ppp/lwip_ppp.c \
SylixOS/net/lwip/tools/rpc/auth_none.c \
SylixOS/net/lwip/tools/rpc/auth_unix.c \
SylixOS/net/lwip/tools/rpc/auth_unix_prot.c \
SylixOS/net/lwip/tools/rpc/bindresvport.c \
SylixOS/net/lwip/tools/rpc/clnt_generic.c \
SylixOS/net/lwip/tools/rpc/clnt_tcp.c \
SylixOS/net/lwip/tools/rpc/clnt_udp.c \
SylixOS/net/lwip/tools/rpc/pmap.c \
SylixOS/net/lwip/tools/rpc/rpc_prot.c \
SylixOS/net/lwip/tools/rpc/xdr.c \
SylixOS/net/lwip/tools/rpc/xdr_mem.c \
SylixOS/net/lwip/tools/rpc/xdr_rec.c \
SylixOS/net/lwip/tools/telnet/lwip_telnet.c \
SylixOS/net/lwip/tools/tftp/lwip_tftp.c \
SylixOS/net/lwip/tools/vpn/lwip_vpn.c \
SylixOS/net/lwip/tools/vpn/lwip_vpnclient.c \
SylixOS/net/lwip/tools/vpn/lwip_vpnnetif.c \
SylixOS/net/lwip/tools/vpn/lwip_vpnshell.c \
SylixOS/net/lwip/unix/af_unix.c \
SylixOS/net/lwip/unix/af_unix_msg.c \
SylixOS/net/lwip/wireless/lwip_wlext.c \
SylixOS/net/lwip/wireless/lwip_wlpriv.c \
SylixOS/net/lwip/wireless/lwip_wlspy.c 

#*********************************************************************************************************
# posix source
#*********************************************************************************************************
POSIX_SRCS = \
SylixOS/posix/aio/aio.c \
SylixOS/posix/aio/aio_lib.c \
SylixOS/posix/dlfcn/dlfcn.c \
SylixOS/posix/execinfo/execinfo.c \
SylixOS/posix/fmtmsg/fmtmsg.c \
SylixOS/posix/fnmatch/fnmatch.c \
SylixOS/posix/mman/mman.c \
SylixOS/posix/mqueue/mqueue.c \
SylixOS/posix/poll/poll.c \
SylixOS/posix/posixLib/pnameLib.c \
SylixOS/posix/posixLib/posixLib.c \
SylixOS/posix/posixLib/posixShell.c \
SylixOS/posix/posixLib/procPosix.c \
SylixOS/posix/pthread/pthread.c \
SylixOS/posix/pthread/pthread_attr.c \
SylixOS/posix/pthread/pthread_barrier.c \
SylixOS/posix/pthread/pthread_cond.c \
SylixOS/posix/pthread/pthread_key.c \
SylixOS/posix/pthread/pthread_mutex.c \
SylixOS/posix/pthread/pthread_rwlock.c \
SylixOS/posix/pthread/pthread_spinlock.c \
SylixOS/posix/resource/resource.c \
SylixOS/posix/sched/sched.c \
SylixOS/posix/semaphore/semaphore.c \
SylixOS/posix/spawn/spawn.c \
SylixOS/posix/sysconf/sysconf.c \
SylixOS/posix/syslog/syslog.c \
SylixOS/posix/timeb/adjtime.c \
SylixOS/posix/timeb/timeb.c \
SylixOS/posix/timeb/times.c \
SylixOS/posix/utsname/utsname.c

#*********************************************************************************************************
# shell source
#*********************************************************************************************************
SHELL_SRCS = \
SylixOS/shell/fsLib/ttinyShellFsCmd.c \
SylixOS/shell/getopt/getopt_bsd.c \
SylixOS/shell/getopt/getopt_var.c \
SylixOS/shell/hashLib/hashHorner.c \
SylixOS/shell/heapLib/ttinyShellHeapCmd.c \
SylixOS/shell/modemLib/ttinyShellModemCmd.c \
SylixOS/shell/tarLib/ttinyShellTarCmd.c \
SylixOS/shell/ttinyShell/ttinyShell.c \
SylixOS/shell/ttinyShell/ttinyShellColor.c \
SylixOS/shell/ttinyShell/ttinyShellLib.c \
SylixOS/shell/ttinyShell/ttinyShellReadline.c \
SylixOS/shell/ttinyShell/ttinyShellSysCmd.c \
SylixOS/shell/ttinyShell/ttinyShellSysVar.c \
SylixOS/shell/ttinyShell/ttinyString.c \
SylixOS/shell/ttinyUser/ttinyUserAdmin.c \
SylixOS/shell/ttinyUser/ttinyUserAuthen.c \
SylixOS/shell/ttinyUser/ttinyUserCache.c \
SylixOS/shell/ttinyVar/ttinyVar.c \
SylixOS/shell/ttinyVar/ttinyVarLib.c

#*********************************************************************************************************
# symbol source
#*********************************************************************************************************
SYMBOL_SRCS = \
SylixOS/symbol/symBsp/symBsp.c \
SylixOS/symbol/symLibc/symLibc.c \
SylixOS/symbol/symTable/symProc.c \
SylixOS/symbol/symTable/symTable.c

#*********************************************************************************************************
# system source
#*********************************************************************************************************
SYS_SRCS = \
SylixOS/system/bus/busSystem.c \
SylixOS/system/device/ata/ata.c \
SylixOS/system/device/ata/ataLib.c \
SylixOS/system/device/block/blockIo.c \
SylixOS/system/device/block/ramDisk.c \
SylixOS/system/device/can/can.c \
SylixOS/system/device/dma/dma.c \
SylixOS/system/device/dma/dmaLib.c \
SylixOS/system/device/eventfd/eventfdDev.c \
SylixOS/system/device/gpio/gpioDev.c \
SylixOS/system/device/gpio/gpioLib.c \
SylixOS/system/device/graph/gmemDev.c \
SylixOS/system/device/hstimerfd/hstimerfdDev.c \
SylixOS/system/device/hwrtc/hwrtc.c \
SylixOS/system/device/i2c/i2cLib.c \
SylixOS/system/device/mem/memDev.c \
SylixOS/system/device/mii/miiDev.c \
SylixOS/system/device/pci/pciDb.c \
SylixOS/system/device/pci/pciLib.c \
SylixOS/system/device/pci/pciProc.c \
SylixOS/system/device/pci/pciScan.c \
SylixOS/system/device/pipe/pipe.c \
SylixOS/system/device/pipe/pipeLib.c \
SylixOS/system/device/pty/pty.c \
SylixOS/system/device/pty/ptyDevice.c \
SylixOS/system/device/pty/ptyHost.c \
SylixOS/system/device/rand/randDev.c \
SylixOS/system/device/rand/randDevLib.c \
SylixOS/system/device/sd/sdLib.c \
SylixOS/system/device/sdcard/client/sdiobaseDrv.c \
SylixOS/system/device/sdcard/client/sdmemory.c \
SylixOS/system/device/sdcard/client/sdmemoryDrv.c \
SylixOS/system/device/sdcard/core/sdcore.c \
SylixOS/system/device/sdcard/core/sdcoreLib.c \
SylixOS/system/device/sdcard/core/sdcrc.c \
SylixOS/system/device/sdcard/core/sddrvm.c \
SylixOS/system/device/sdcard/core/sdiocoreLib.c \
SylixOS/system/device/sdcard/core/sdiodrvm.c \
SylixOS/system/device/sdcard/core/sdutil.c \
SylixOS/system/device/sdcard/host/sdhci.c \
SylixOS/system/device/shm/shm.c \
SylixOS/system/device/spi/spiLib.c \
SylixOS/system/device/spipe/spipe.c \
SylixOS/system/device/spipe/spipeLib.c \
SylixOS/system/device/ty/termios.c \
SylixOS/system/device/ty/tty.c \
SylixOS/system/device/ty/tyLib.c \
SylixOS/system/epoll/epollDev.c \
SylixOS/system/epoll/epollLib.c \
SylixOS/system/excLib/excLib.c \
SylixOS/system/hotplugLib/hotplugDev.c \
SylixOS/system/hotplugLib/hotplugLib.c \
SylixOS/system/ioLib/ioDir.c \
SylixOS/system/ioLib/ioFcntl.c \
SylixOS/system/ioLib/ioFdNode.c \
SylixOS/system/ioLib/ioFifo.c \
SylixOS/system/ioLib/ioFile.c \
SylixOS/system/ioLib/ioFormat.c \
SylixOS/system/ioLib/ioInterface.c \
SylixOS/system/ioLib/ioLib.c \
SylixOS/system/ioLib/ioLicense.c \
SylixOS/system/ioLib/ioLockF.c \
SylixOS/system/ioLib/ioPath.c \
SylixOS/system/ioLib/ioShow.c \
SylixOS/system/ioLib/ioStat.c \
SylixOS/system/ioLib/ioSymlink.c \
SylixOS/system/ioLib/ioSys.c \
SylixOS/system/logLib/logLib.c \
SylixOS/system/pm/pmAdapter.c \
SylixOS/system/pm/pmDev.c \
SylixOS/system/pm/pmIdle.c \
SylixOS/system/pm/pmSystem.c \
SylixOS/system/ptimer/ptimer.c \
SylixOS/system/ptimer/ptimerDev.c \
SylixOS/system/select/selectCtl.c \
SylixOS/system/select/selectInit.c \
SylixOS/system/select/selectLib.c \
SylixOS/system/select/selectList.c \
SylixOS/system/select/selectNode.c \
SylixOS/system/select/selectPty.c \
SylixOS/system/select/selectTy.c \
SylixOS/system/select/waitfile.c \
SylixOS/system/signal/signal.c \
SylixOS/system/signal/signalDev.c \
SylixOS/system/signal/signalEvent.c \
SylixOS/system/signal/signalJmp.c \
SylixOS/system/signal/signalLib.c \
SylixOS/system/sysHookList/sysHookList.c \
SylixOS/system/sysHookList/sysHookListLib.c \
SylixOS/system/sysInit/sysInit.c \
SylixOS/system/threadpool/threadpool.c \
SylixOS/system/threadpool/threadpoolLib.c \
SylixOS/system/util/bmsgLib.c \
SylixOS/system/util/rngLib.c

#*********************************************************************************************************
# cplusplus source
#*********************************************************************************************************
CPP_SRCS = \
SylixOS/cplusplus/cppRtLib/cppEabiLib.cpp \
SylixOS/cplusplus/cppRtLib/cppMemLib.cpp \
SylixOS/cplusplus/cppRtLib/cppRtBegin.cpp \
SylixOS/cplusplus/cppRtLib/cppRtEnd.cpp \
SylixOS/cplusplus/cppRtLib/cppSupLib.cpp

#*********************************************************************************************************
# all libsylixos source
#*********************************************************************************************************
SRCS  = $(APPL_SRCS)
SRCS += $(ARCH_SRCS)
SRCS += $(DEBUG_SRCS)
SRCS += $(FS_SRCS)
SRCS += $(GUI_SRCS)
SRCS += $(KERN_SRCS)
SRCS += $(LIB_SRCS)
SRCS += $(MONITOR_SRCS)
SRCS += $(LOADER_SRCS)
SRCS += $(MPI_SRCS)
SRCS += $(NET_SRCS)
SRCS += $(POSIX_SRCS)
SRCS += $(SHELL_SRCS)
SRCS += $(SYMBOL_SRCS)
SRCS += $(SYS_SRCS)
SRCS += $(CPP_SRCS)

#*********************************************************************************************************
# libdsohandle source
#*********************************************************************************************************
DSOH_SRCS = \
SylixOS/dsohandle/dsohandle.c

#*********************************************************************************************************
# libvpmpdm source
#*********************************************************************************************************
VPMPDM_SRCS = \
SylixOS/vpmpdm/vpmpdm_backtrace.c \
SylixOS/vpmpdm/vpmpdm_cpp.cpp \
SylixOS/vpmpdm/vpmpdm_lm.c \
SylixOS/vpmpdm/vpmpdm_start.c \
SylixOS/vpmpdm/vpmpdm_stdio.c \
SylixOS/vpmpdm/vpmpdm.c

#*********************************************************************************************************
# libxinput source
#*********************************************************************************************************
XINPUT_SRCS = \
SylixOS/xinput/xdev.c \
SylixOS/xinput/xinput.c \
SylixOS/xinput/xproc.c

#*********************************************************************************************************
# libxsiipc source
#*********************************************************************************************************
XSIIPC_SRCS = \
SylixOS/xsiipc/msg.c \
SylixOS/xsiipc/proc.c \
SylixOS/xsiipc/sem.c \
SylixOS/xsiipc/shm.c \
SylixOS/xsiipc/xsiipc.c

#*********************************************************************************************************
# build path
#*********************************************************************************************************
ifeq ($(DEBUG_LEVEL), debug)
OUTDIR = Debug
else
OUTDIR = Release
endif

OUTPATH = ./$(OUTDIR)
OBJPATH = $(OUTPATH)/obj
DEPPATH = $(OUTPATH)/dep

#*********************************************************************************************************
# libsylixos target
#*********************************************************************************************************
TARGET = $(OUTPATH)/libsylixos.a

#*********************************************************************************************************
# libdsohandle target
#*********************************************************************************************************
DSOH_TARGET = $(OUTPATH)/libdsohandle.a

#*********************************************************************************************************
# libvpmpdm target
#*********************************************************************************************************
VPMPDM_A_TARGET = $(OUTPATH)/libvpmpdm.a
VPMPDM_S_TARGET = $(OUTPATH)/libvpmpdm.so

#*********************************************************************************************************
# libxinput target
#*********************************************************************************************************
XINPUT_TARGET = $(OUTPATH)/xinput.ko

#*********************************************************************************************************
# libxsiipc target
#*********************************************************************************************************
XSIIPC_TARGET = $(OUTPATH)/xsiipc.ko

#*********************************************************************************************************
# libsylixos objects
#*********************************************************************************************************
OBJS_APPL    = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(APPL_SRCS))))
OBJS_ARCH    = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(ARCH_SRCS))))
OBJS_DEBUG   = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(DEBUG_SRCS))))
OBJS_FS      = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(FS_SRCS))))
OBJS_GUI     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(GUI_SRCS))))
OBJS_KERN    = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(KERN_SRCS))))
OBJS_LIB     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(LIB_SRCS))))
OBJS_LOADER  = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(LOADER_SRCS))))
OBJS_MONITOR = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(MONITOR_SRCS))))
OBJS_MPI     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(MPI_SRCS))))
OBJS_NET     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(NET_SRCS))))
OBJS_POSIX   = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(POSIX_SRCS))))
OBJS_SHELL   = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(SHELL_SRCS))))
OBJS_SYMBOL  = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(SYMBOL_SRCS))))
OBJS_SYS     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(SYS_SRCS))))
OBJS_CPP     = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(CPP_SRCS))))
OBJS         = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(SRCS))))
DEPS         = $(addprefix $(DEPPATH)/, $(addsuffix .d, $(basename $(SRCS))))

#*********************************************************************************************************
# libdsohandle objects
#*********************************************************************************************************
OBJS_DSOH    = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(DSOH_SRCS))))
DEPS_DSOH    = $(addprefix $(DEPPATH)/, $(addsuffix .d, $(basename $(DSOH_SRCS))))

#*********************************************************************************************************
# libvpmpdm objects
#*********************************************************************************************************
OBJS_VPMPDM  = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(VPMPDM_SRCS))))
DEPS_VPMPDM  = $(addprefix $(DEPPATH)/, $(addsuffix .d, $(basename $(VPMPDM_SRCS))))

#*********************************************************************************************************
# libxinput objects
#*********************************************************************************************************
OBJS_XINPUT  = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(XINPUT_SRCS))))
DEPS_XINPUT  = $(addprefix $(DEPPATH)/, $(addsuffix .d, $(basename $(XINPUT_SRCS))))

#*********************************************************************************************************
# libxsiipc objects
#*********************************************************************************************************
OBJS_XSIIPC  = $(addprefix $(OBJPATH)/, $(addsuffix .o, $(basename $(XSIIPC_SRCS))))
DEPS_XSIIPC  = $(addprefix $(DEPPATH)/, $(addsuffix .d, $(basename $(XSIIPC_SRCS))))

#*********************************************************************************************************
# include path
#*********************************************************************************************************
INCDIR  = "./SylixOS"
INCDIR += "./SylixOS/include"
INCDIR += "./SylixOS/include/inet"

#*********************************************************************************************************
# compiler preprocess
#*********************************************************************************************************
DSYMBOL  = -DSYLIXOS

#*********************************************************************************************************
# compiler optimize
#*********************************************************************************************************
ifeq ($(DEBUG_LEVEL), debug)
OPTIMIZE = -O0 -g3 -gdwarf-2
else
OPTIMIZE = -O2 -Os -g1 -gdwarf-2										#you can try use O3
endif

#*********************************************************************************************************
# rm command parameter
#*********************************************************************************************************
UNAME = $(shell uname -sm)

ifneq (,$(findstring Linux, $(UNAME)))
HOST_OS = linux
endif
ifneq (,$(findstring Darwin, $(UNAME)))
HOST_OS = darwin
endif
ifneq (,$(findstring Macintosh, $(UNAME)))
HOST_OS = darwin
endif
ifneq (,$(findstring CYGWIN, $(UNAME)))
HOST_OS = windows
endif
ifneq (,$(findstring windows, $(UNAME)))
HOST_OS = windows
endif

ifeq ($(HOST_OS),)
$(error Unable to determine HOST_OS from uname -sm: $(UNAME)!)
endif

ifeq ($(HOST_OS), windows) 
RM_PARAM = -rdf
else
RM_PARAM = -rf
endif

#*********************************************************************************************************
# depends and compiler parameter (cplusplus in kernel MUST NOT use exceptions and rtti)
#*********************************************************************************************************
DEPENDFLAG  = -MM
CXX_EXCEPT  = -fno-exceptions -fno-rtti
COMMONFLAGS = $(CPUFLAGS) $(OPTIMIZE) -Wall -fmessage-length=0 -fsigned-char -fno-short-enums
ASFLAGS     = -x assembler-with-cpp $(DSYMBOL) $(addprefix -I,$(INCDIR)) $(COMMONFLAGS) -c
CFLAGS      = $(DSYMBOL) $(addprefix -I,$(INCDIR)) $(COMMONFLAGS) -c
CXXFLAGS    = $(DSYMBOL) $(addprefix -I,$(INCDIR)) $(CXX_EXCEPT) $(COMMONFLAGS) -c
ARFLAGS     = -r

#*********************************************************************************************************
# define some useful variable
#*********************************************************************************************************
DEPEND          = $(CC)  $(DEPENDFLAG) $(CFLAGS)
DEPEND.d        = $(subst -g ,,$(DEPEND))
COMPILE.S       = $(AS)  $(ASFLAGS)
COMPILE_VFP.S   = $(AS)  $(ASFLAGS) $(FPUFLAGS)
COMPILE.c       = $(CC)  $(CFLAGS)
COMPILE.cxx     = $(CXX) $(CXXFLAGS)

#*********************************************************************************************************
# compile -fPIC
#*********************************************************************************************************
COMPILE_PIC.c   = $(COMPILE.c) -fPIC
COMPILE_PIC.cxx = $(COMPILE.cxx) -fPIC

#*********************************************************************************************************
# target
#*********************************************************************************************************
ifeq ($(BUILD_PROCESS_SUP_LIB), 1)
ifeq ($(BUILD_KERNEL_MODULE), 1)
all: $(TARGET) $(DSOH_TARGET) $(VPMPDM_A_TARGET) $(VPMPDM_S_TARGET) $(XINPUT_TARGET) $(XSIIPC_TARGET)
		@echo create "$(TARGET) $(DSOH_TARGET) $(VPMPDM_A_TARGET) $(VPMPDM_S_TARGET)" \
		"$(XINPUT_TARGET) $(XSIIPC_TARGET)" success.
else
all: $(TARGET) $(DSOH_TARGET) $(VPMPDM_A_TARGET) $(VPMPDM_S_TARGET)
		@echo create "$(TARGET) $(DSOH_TARGET) $(VPMPDM_A_TARGET) $(VPMPDM_S_TARGET)" success.
endif
else
all: $(TARGET)
		@echo create "$(TARGET)" success.
endif

#*********************************************************************************************************
# include depends
#*********************************************************************************************************
ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), clean_project)
sinclude $(DEPS)
endif
endif

#*********************************************************************************************************
# create depends files
#*********************************************************************************************************
$(DEPPATH)/%.d: %.c
		@echo creating $@
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		@rm -f $@; \
		echo -n '$@ $(addprefix $(OBJPATH)/, $(dir $<))' > $@; \
		$(DEPEND.d) $< >> $@ || rm -f $@; exit;

$(DEPPATH)/%.d: %.cpp
		@echo creating $@
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		@rm -f $@; \
		echo -n '$@ $(addprefix $(OBJPATH)/, $(dir $<))' > $@; \
		$(DEPEND.d) $< >> $@ || rm -f $@; exit;

#*********************************************************************************************************
# compile source files
#*********************************************************************************************************
$(OBJPATH)/%.o: %.S
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE.S) $< -o $@

$(OBJPATH)/%.o: %.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE.c) $< -o $@

$(OBJPATH)/%.o: %.cpp
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE.cxx) $< -o $@

#*********************************************************************************************************
# compile VFP source files
#*********************************************************************************************************
$(OBJPATH)/SylixOS/arch/arm/fpu/vfp9/armVfp9Asm.o: ./SylixOS/arch/arm/fpu/vfp9/armVfp9Asm.S
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_VFP.S) $< -o $@

$(OBJPATH)/SylixOS/arch/arm/fpu/vfp11/armVfp11Asm.o: ./SylixOS/arch/arm/fpu/vfp11/armVfp11Asm.S
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_VFP.S) $< -o $@

$(OBJPATH)/SylixOS/arch/arm/fpu/vfpv3/armVfpV3Asm.o: ./SylixOS/arch/arm/fpu/vfpv3/armVfpV3Asm.S
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_VFP.S) $< -o $@

#*********************************************************************************************************
# link libsylixos.a object files
#*********************************************************************************************************
$(TARGET): $(OBJS)
		-rm $(RM_PARAM) $(TARGET)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_APPL)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_ARCH)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_DEBUG)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_FS)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_GUI)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_KERN)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_LIB)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_MONITOR)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_LOADER)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_MPI)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_NET)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_POSIX)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_SHELL)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_SYMBOL)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_SYS)
		$(AR) $(ARFLAGS) $(TARGET) $(OBJS_CPP)

#*********************************************************************************************************
# link libdsohandle.a object files
#*********************************************************************************************************
$(DSOH_TARGET): $(OBJS_DSOH)
		$(AR) $(ARFLAGS) $(DSOH_TARGET) $(OBJS_DSOH)

#*********************************************************************************************************
# compile PIC code
#*********************************************************************************************************
$(OBJPATH)/SylixOS/vpmpdm/vpmpdm_backtrace.o: ./SylixOS/vpmpdm/vpmpdm_backtrace.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.c) $< -o $@

$(OBJPATH)/SylixOS/vpmpdm/vpmpdm_lm.o: ./SylixOS/vpmpdm/vpmpdm_lm.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.c) $< -o $@

$(OBJPATH)/SylixOS/vpmpdm/vpmpdm_start.o: ./SylixOS/vpmpdm/vpmpdm_start.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.c) $< -o $@

$(OBJPATH)/SylixOS/vpmpdm/vpmpdm_stdio.o: ./SylixOS/vpmpdm/vpmpdm_stdio.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.c) $< -o $@

$(OBJPATH)/SylixOS/vpmpdm/vpmpdm.o: ./SylixOS/vpmpdm/vpmpdm.c
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.c) $< -o $@

$(OBJPATH)/SylixOS/vpmpdm/vpmpdm_cpp.o: ./SylixOS/vpmpdm/vpmpdm_cpp.cpp
		@if [ ! -d "$(dir $@)" ]; then mkdir -p "$(dir $@)"; fi
		$(COMPILE_PIC.cxx) $< -o $@

#*********************************************************************************************************
# link libvpmpdm.a object files
#*********************************************************************************************************
$(VPMPDM_A_TARGET): $(OBJS_VPMPDM)
		$(AR) $(ARFLAGS) $(VPMPDM_A_TARGET) $(OBJS_VPMPDM)

#*********************************************************************************************************
# link libvpmpdm.so object files
#*********************************************************************************************************
$(VPMPDM_S_TARGET): $(OBJS_VPMPDM)
		$(LD) $(CPUFLAGS) -nostdlib -fPIC -shared -o $(VPMPDM_S_TARGET) $(OBJS_VPMPDM) -lgcc

#*********************************************************************************************************
# link xinput.ko object files
#*********************************************************************************************************
$(XINPUT_TARGET): $(OBJS_XINPUT)
		$(LD) $(CPUFLAGS) -nostdlib -r -o $(XINPUT_TARGET) $(OBJS_XINPUT) -lm -lgcc

#*********************************************************************************************************
# link xsiipc.ko object files
#*********************************************************************************************************
$(XSIIPC_TARGET): $(OBJS_XSIIPC)
		$(LD) $(CPUFLAGS) -nostdlib -r -o $(XSIIPC_TARGET) $(OBJS_XSIIPC) -lm -lgcc

#*********************************************************************************************************
# clean
#*********************************************************************************************************
.PHONY: clean
.PHONY: clean_project

#*********************************************************************************************************
# clean objects
#*********************************************************************************************************
clean:
		-rm $(RM_PARAM) $(TARGET)
		-rm $(RM_PARAM) $(DSOH_TARGET)
		-rm $(RM_PARAM) $(VPMPDM_A_TARGET)
		-rm $(RM_PARAM) $(VPMPDM_S_TARGET)
		-rm $(RM_PARAM) $(XINPUT_TARGET)
		-rm $(RM_PARAM) $(XSIIPC_TARGET)
		-rm $(RM_PARAM) $(OBJPATH)

#*********************************************************************************************************
# clean project
#*********************************************************************************************************
clean_project:
		-rm $(RM_PARAM) $(OUTPATH)

#*********************************************************************************************************
# END
#*********************************************************************************************************
