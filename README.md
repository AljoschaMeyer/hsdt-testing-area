# HSDT Implementation

An implementation to experiment with and benchmark different drafts of the hsdt data format, as discussed on [ssb](%y5G9E1MJ8sv4NSyQ+T8PszTTPEcf1j7vPkcHSR3AuXA=.sha256).

Currently uses `HSDT_HOMOGENOUS_ARRAYS` to turn on the optimization for arrays with items of homogenous type, and `HSDT_SIZE_IN_BYTES` to use the absolute size of collection items in bytes as a prefix, rather than the number of items.
