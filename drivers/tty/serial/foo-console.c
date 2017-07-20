/*
 * Tiny TTY driver
 *
 * Copyright (C) 2002 Greg Kroah-Hartman (greg@kroah.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 * This driver shows how to create a minimal serial driver using the 2.5
 * kernel's serial driver layer. It does not rely on any backing hardware, but
 * creates a timer that emulates data being received from some kind of
 * hardware.
 *
 * Compiled this driver with:

  CFLAGS = -Wall -O2 -fomit-frame-pointer -DMODULE -D__KERNEL__
  IDIR = /path/to/linux-2.5/include/goes/here

  tiny_serial.o::tiny_serial.c
    $(CC) $(CFLAGS) -I$(IDIR) -c -o $@ $<
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/module.h>


#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC "Tiny serial driver"

/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define DELAY_TIME    HZ * 2  /* 2 seconds per character */
#define TINY_DATA_CHARACTER 't'

#define TINY_SERIAL_MAJOR 240 /* experimental range */
#define TINY_SERIAL_MINORS  1 /* only have one minor */
#define UART_NR     1 /* only use one port */

#define TINY_SERIAL_NAME  "ttytiny"

#define MY_NAME     TINY_SERIAL_NAME

#define dbg(fmt, arg...)            \
  do {                \
    if (debug)            \
      printk (KERN_DEBUG "%s: %s: " fmt "\n",   \
        MY_NAME , __FUNCTION__ , ## arg); \
  } while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format "\n" , MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format "\n" , MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format "\n" , MY_NAME , ## arg)


static int debug;
// MODULE_PARM(debug, int, "i");
// MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

static struct timer_list *timer;

static void tiny_stop_tx(struct uart_port *port)
{
  dbg ();
}

static void tiny_stop_rx(struct uart_port *port)
{
  dbg ();
}

static void tiny_enable_ms(struct uart_port *port)
{
  dbg ();
}

static void tiny_tx_chars(struct uart_port *port)
{
  struct circ_buf *xmit = &port->state->xmit;
  int count;

  dbg ();
  if (port->x_char) {
    dbg ("wrote %2x", port->x_char);
    port->icount.tx++;
    port->x_char = 0;
    return;
  }
  if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
    tiny_stop_tx(port);
    return;
  }

  count = port->fifosize >> 1;
  do {
    dbg ("wrote %2x", xmit->buf[xmit->tail]);
    xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
    port->icount.tx++;
    if (uart_circ_empty(xmit))
      break;
  } while (--count > 0);

  if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
    uart_write_wakeup(port);

  if (uart_circ_empty(xmit))
    tiny_stop_tx(port);
}

static void tiny_start_tx(struct uart_port *port)
{
  dbg ();
}

static void tiny_timer (unsigned long data)
{
  struct uart_port *port;
  struct tty_port *tty;

  dbg ();

  port = (struct uart_port *)data;
  if (!port)
    return;
  if (!port->state)
    return;
  tty = &port->state->port;

  /* add one character to the tty port */
  /* this doesn't actually push the data through unless tty->low_latency is set */
  tty_insert_flip_char(tty, TINY_DATA_CHARACTER, 0);

  tty_flip_buffer_push(tty);

  /* resubmit the timer again */
  timer->expires = jiffies + DELAY_TIME;
  add_timer (timer);

  /* see if we have any data to transmit */
  tiny_tx_chars(port);
}

static unsigned int tiny_tx_empty(struct uart_port *port)
{
  return 0;
}

static unsigned int tiny_get_mctrl(struct uart_port *port)
{
  return 0;
}

static void tiny_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void tiny_break_ctl(struct uart_port *port, int break_state)
{
}

static int tiny_startup(struct uart_port *port)
{
  /* this is the first time this port is opened */
  /* do any hardware initialization needed here */

  /* create our timer and submit it */
  if (!timer) {
    timer = kmalloc (sizeof (*timer), GFP_KERNEL);
    if (!timer)
      return -ENOMEM;
  }
  timer->data = (unsigned long )port;
  timer->expires = jiffies + DELAY_TIME;
  timer->function = tiny_timer;
  add_timer (timer);
  return 0;
}

static void tiny_shutdown(struct uart_port *port)
{
  /* The port is being closed by the last user. */
  /* Do any hardware specific stuff here */

  /* shut down our timer */
  del_timer (timer);
}

static const char *tiny_type(struct uart_port *port)
{
  return "tinytty";
}

static void tiny_release_port(struct uart_port *port)
{

}

static int tiny_request_port(struct uart_port *port)
{
  return 0;
}

static void tiny_config_port(struct uart_port *port, int flags)
{
}

static int tiny_verify_port(struct uart_port *port, struct serial_struct *ser)
{
  return 0;
}

static struct uart_ops tiny_ops = {
  .tx_empty = tiny_tx_empty,
  .set_mctrl  = tiny_set_mctrl,
  .get_mctrl  = tiny_get_mctrl,
  .stop_tx  = tiny_stop_tx,
  .start_tx = tiny_start_tx,
  .stop_rx  = tiny_stop_rx,
  .enable_ms  = tiny_enable_ms,
  .break_ctl  = tiny_break_ctl,
  .startup  = tiny_startup,
  .shutdown = tiny_shutdown,
  .type   = tiny_type,
  .release_port = tiny_release_port,
  .request_port = tiny_request_port,
  .config_port  = tiny_config_port,
  .verify_port  = tiny_verify_port,
};

static struct uart_port tiny_port = {
  .ops    = &tiny_ops,
};

static struct uart_driver tiny_reg = {
  .owner    = THIS_MODULE,
  .driver_name  = TINY_SERIAL_NAME,
  .dev_name = TINY_SERIAL_NAME,
  .major    = TINY_SERIAL_MAJOR,
  .minor    = TINY_SERIAL_MINORS,
  .nr   = UART_NR,
};


static int __init tiny_init(void)
{
  int result;

  info ("Tiny serial driver");

  result = uart_register_driver(&tiny_reg);
  if (result)
    return result;

  result = uart_add_one_port(&tiny_reg, &tiny_port);
  if (result)
    uart_unregister_driver(&tiny_reg);

  return result;
}

module_init (tiny_init);
