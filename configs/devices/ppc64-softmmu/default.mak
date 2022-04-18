# Default configuration for ppc64-softmmu

# Include all 32-bit boards
include ../ppc-softmmu/default.mak

# For PowerNV
CONFIG_POWERNV=y

# For pSeries
CONFIG_PSERIES=y

CONFIG_SEMIHOSTING=y
CONFIG_ARM_COMPATIBLE_SEMIHOSTING=y
