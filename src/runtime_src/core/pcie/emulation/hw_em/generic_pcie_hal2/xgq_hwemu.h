/*
 *  Copyright (C) 2021, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#pragma once

#include "xgq.h"

#include <list>
#include <boost/pool/object_pool.hpp>

namespace xclhwemhal2 {
  class HwEmShim;
}

#define XRT_QUEUE1_SUB_BASE    0x80000000
#define XRT_QUEUE1_COM_BASE    (XRT_QUEUE1_SUB_BASE + XRT_SUB_Q1_SLOT_SIZE * XRT_QUEUE1_SLOT_NUM)

#define XRT_XGQ_SUB_BASE       0x1040000
#define XRT_XGQ_COM_BASE       0x1030000

namespace hwemu {

  //! Forward declaration
  class xgq_cmd;
  class xocl_xgq;

  /**
   * class xgq_queue: Represent a XRT Generic Queue pair.
   *
   * @submit_worker():          submission queue worker thread
   * @complete_worker():        completion queue worker thread
   * @update_doorbell():        update sub_tail to submission XGQ doorbell
   * @check_doorbell():         check com_tail in completion XGQ doorbell
   * @submit_cmd():             put a command into submission queue entry
   * @read_completion():        read a completion entry from completion queue
   * @clear_sub_slot_state():   clear the first word of a submission queue
   *                            entry so that consumer can wait for the state
   *                            field for next command in this slot
   * @iowrite32():              write 32 bits to an IO address
   * @ioread32():               read 32 bits value from an IO address
   *
   * @pending_cmds:             a list of commands waiting to be submitted
   * @submitted_cmds:           a hash map of commands sent but not yet
   *                            completed
   */
  class xgq_queue
  {
    public:
      xgq_queue(xclhwemhal2::HwEmShim*, xocl_xgq*, uint16_t, uint32_t, uint64_t, uint64_t, uint64_t, uint64_t);
      ~xgq_queue();

      xclhwemhal2::HwEmShim*   device;
      xocl_xgq*                xgqp;

      int      submit_worker();
      int      complete_worker();
      void     update_doorbell();
      uint16_t check_doorbell();
      int      submit_cmd(xgq_cmd *xcmd);
      void     read_completion(xrt_com_queue_entry& ccmd);
      void     clear_sub_slot_state(uint64_t sub_slot);
      void     iowrite32(uint32_t addr, uint32_t data);
      uint32_t ioread32(uint32_t addr);

      uint16_t        qid;
      uint16_t        nslot;
      uint32_t        slot_size;

      uint64_t        xgq_sub_base;
      uint64_t        xgq_com_base;
      uint64_t        sub_head;
      uint64_t        sub_tail;
      uint64_t        sub_base;
      uint64_t        com_head;
      uint64_t        com_tail;
      uint64_t        com_base;

      std::list<xgq_cmd*>          pending_cmds;
      std::map<uint64_t, xgq_cmd*> submitted_cmds;
      std::mutex                   queue_mutex;
      bool                         stop;

      std::thread*            sub_thread;
      std::condition_variable sub_cv;
      std::thread*            com_thread;
      std::condition_variable com_cv;
  };

  /**
   * class xgq_cmd: Represent a command in XGQ. It contains the execbuf sent
   *                from the xclExecBuf (ert_packet BO) and XGQ command.
   *
   * @opcode():          ert_packet opcode
   * @set_state():       set ert_packet state
   * @convert_bo():      convert ert_packet to XGQ command packet
   * @payload_size():    ert_packet payload size in bytes
   * @xcmd_size():       XGQ command total size in bytes
   */
  class xgq_cmd
  {
    public:
      xgq_cmd();

      uint32_t    opcode();
      void        set_state(enum ert_cmd_state state);
      int         convert_bo(xclemulation::drm_xocl_bo *bo);
      uint32_t    payload_size();
      uint32_t    xcmd_size();

      xocl_xgq              *xgqp;
      uint16_t              cmdid;
      std::vector<uint32_t> sq_buf;
      struct ert_packet     *ert_pkt;

      //! Static member varibale
      //  to get the unique ID for each command
      static uint64_t next_uid;
  };

  /**
   * class xocl_xgq:    The main class of XGQ.
   *
   * @add_exec_buffer:  Convert an exec buf to XGQ command, add it to XGQ
   *                    pending command list and notify XGQ thread.
   */
  class xocl_xgq
  {
    public:
      xocl_xgq(xclhwemhal2::HwEmShim* dev);
      ~xocl_xgq();

      int    add_exec_buffer(xclemulation::drm_xocl_bo *buf);

      // TODO support multiple queues
      xgq_queue queue;

      boost::object_pool<xgq_cmd> cmd_pool;

      xclhwemhal2::HwEmShim*   device;
  };

}  // namespace hwemu
