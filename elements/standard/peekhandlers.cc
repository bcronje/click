/*
 * peekhandlers.{cc,hh} -- element runs read handlers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "peekhandlers.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>

PeekHandlers::PeekHandlers()
  : _timer(timer_hook, this)
{
  MOD_INC_USE_COUNT;
}

PeekHandlers::~PeekHandlers()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

void
PeekHandlers::push_command(Element *e, int hid, int extra)
{
  _h_element.push_back(e);
  _h_hid.push_back(hid);
  _h_extra.push_back(extra);
}

int
PeekHandlers::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
PeekHandlers::do_configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _h_element.clear();
  _h_hid.clear();
  _h_extra.clear();
  
  for (int i = 0; i < conf.size(); i++) {
    Element *e;
    int extra;

    if (!conf[i])
      continue;
    else if (cp_seconds_as_milli(conf[i], reinterpret_cast<uint32_t *>(&extra))) {
      push_command(0, HID_WAIT, extra);
      continue;
    } else if (conf[i] == "quit") {
      push_command(0, HID_QUIT, 0);
      if (i < conf.size() - 1)
	errh->warning("arguments after `quit' directive ignored");
      break;
    } else if (conf[i] == "loop") {
      push_command(0, HID_LOOP, 0);
      if (i < conf.size() - 1)
	errh->warning("arguments after `loop' directive ignored");
      break;
    } else if (cp_handler(conf[i], this, true, false, &e, &extra, errh)) {
      push_command(e, extra, 0);
      continue;
    }
    
    errh->error("argument %d: expected `TIMEOUT' or `ELEMENT.HANDLER'",
		i + 1);
  }

  return 0;
}

int
PeekHandlers::initialize(ErrorHandler *errh)
{
  // configure now, since we have read handlers
  Vector<String> conf;
  configuration(conf);
  if (do_configure(conf, errh) < 0)
    return -1;
  
  _pos = 0;
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void
PeekHandlers::uninitialize()
{
  _timer.unschedule();
}


void
PeekHandlers::timer_hook(Timer *, void *thunk)
{
  PeekHandlers *peek = (PeekHandlers *)thunk;
  ErrorHandler *errh = ErrorHandler::default_handler();
  Router *router = peek->router();

  int pos = peek->_pos;
  while (pos < peek->_h_element.size()) {
    Element *he = peek->_h_element[pos];
    int hid = peek->_h_hid[pos];

    if (hid == HID_WAIT) {
      peek->_timer.schedule_after_ms(peek->_h_extra[pos]);
      break;
    } else if (hid == HID_QUIT) {
      router->please_stop_driver();
      break;
    } else if (hid == HID_LOOP) {
      pos = -1;
      // allow progress even if there is no delay in loop
      peek->_timer.schedule_now();
      break;
    }
    
    const Router::Handler &rh = router->handler(hid);
    String value = rh.call_read(he);
    errh->message("%s:\n%s\n", rh.unparse_name(he).cc(), value.cc());
    pos++;
  }

  peek->_pos = pos + 1;
}

EXPORT_ELEMENT(PeekHandlers)
