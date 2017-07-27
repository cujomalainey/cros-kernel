/*
 * include/sound/gdb-dsp-tty.h
 *
 * Copyright (C) 2017 by Curtis Malainey.
 *
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL)
 */

// for now assume 1 DSP active at any time, future add N dsps
struct gdb_dsp;

struct gdb_dsp_ops {
  int (*write)(struct gdb_dsp *gdev, const unsigned char *data, int size);
  int (*write_room)(struct gdb_dsp *gdev);
};

struct gdb_dsp {
  const char *name;
  struct gdb_dsp_ops *ops;
  void *prv_device;
};

int register_gdb_dsp(struct gdb_dsp *new_dsp);

int write_gdp_dsp_debug(const unsigned char *data, int size);
