CONFIG_PCI=y
CONFIG_VIRTIO_PCI=$(CONFIG_PCI)
CONFIG_VHOST_USER_SCSI=$(call land,$(CONFIG_VHOST_USER),$(CONFIG_LINUX))
CONFIG_VIRTIO=y
CONFIG_SCLPCONSOLE=y
CONFIG_TERMINAL3270=y
CONFIG_S390_FLIC=y
CONFIG_S390_FLIC_KVM=$(CONFIG_KVM)
CONFIG_VFIO_CCW=$(CONFIG_LINUX)
CONFIG_WDT_DIAG288=y
