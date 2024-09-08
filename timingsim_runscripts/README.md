## Setup
Run setup_smarts_runs.py to generate runscripts for each run in EX_DIR_HIER.
Launching the runs via resulting scripts is not automated, as runs are resource heavy and should be provisioned instead of get launched all at once.

### configurations used for each figure:
• Figure 8:
StarNUMA_main
baseline_main

• Figure9:
STATIC_ALLOC (runs both baseline and STARNUMA)

• Figure 10:
StarNUMA_270ns
baseline_main

• Figure 11:
StarNUMA_main
StarNUMA_halfBW
baseline_ISOBW
baseline_2XBW

• Figure 12:
StarNUMA_smallpool
baseline_main
