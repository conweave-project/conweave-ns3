#!/bin/bash

rm -rf ./mix/output/*
echo "date,id,ccmode,lbmode,cwh_tx_expiry_time,cwh_extra_reply_deadline,cwh_path_pause_time,cwh_extra_voq_flush_time,cwh_default_voq_waiting_time,pfc,irn,has_win,var_win,topo,bw,cdf,load,time" > ./mix/.history

rm -rf ./analysis/figures/*.pdf