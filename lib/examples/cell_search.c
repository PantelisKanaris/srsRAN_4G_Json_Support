/**
 * Copyright 2013-2023 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <cjson/cJSON.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include <unistd.h>

#include "srsran/srsran.h"

#include "srsran/common/crash_handler.h"
#include "srsran/phy/rf/rf_utils.h"

#ifndef DISABLE_RF
#include "srsran/phy/rf/rf.h"
#endif

#define MHZ 1000000
#define SAMP_FREQ 1920000
#define FLEN 9600
#define FLEN_PERIOD 0.005
bool test_mode = false;
#define MAX_EARFCN 1000

int band         = -1;
int earfcn_start = -1, earfcn_end = -1;

cell_search_cfg_t cell_detect_config = {.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH,
                                        .max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS,
                                        .nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES,
                                        .init_agc             = 0,
                                        .force_tdd            = false};

struct cells {
  srsran_cell_t cell;
  float         freq;
  int           dl_earfcn;
  float         power;
};
struct cells results[1024];

float rf_gain = 70.0;
char* rf_args = "";
char* rf_dev  = "";

void SerializeCell(struct cells  * cell)
{
  cJSON* root = cJSON_CreateObject();
  if (root == NULL) {
    printf("ERROR: Json serializer failed to initialize \n");
    exit(-1);
  }
  cJSON_AddNumberToObject(root, "cell_id", cell->cell.id);
  cJSON_AddNumberToObject(root, "freq_khz", cell->freq);
  cJSON_AddNumberToObject(root, "earfcn", cell->dl_earfcn);
  cJSON_AddNumberToObject(root, "prb", cell->cell.nof_prb);
  cJSON_AddNumberToObject(root, "ports", cell->cell.nof_ports);
  cJSON_AddNumberToObject(root, "pss_power", srsran_convert_power_to_dB(cell->power));
  char* json_str = cJSON_Print(root);
  if (json_str == NULL) {
    printf("ERROR: Json serializer failed to  \n");
    exit(-1);
  }

  FILE* fp = fopen("cell_info.json", "a");
  if (fp) {
    fputs(json_str, fp);
    fclose(fp);
  }
  free(json_str);
  cJSON_Delete(root);
}

void SerializeCells(struct cells* cells, int n_cells)
{
  cJSON* root = cJSON_CreateObject();
  cJSON* cell_array = cJSON_CreateArray();

  for (int i = 0; i < n_cells; i++) {
    cJSON* cell_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(cell_obj, "cell_id", cells[i].cell.id);
    cJSON_AddNumberToObject(cell_obj, "freq_khz", cells[i].freq);
    cJSON_AddNumberToObject(cell_obj, "earfcn", cells[i].dl_earfcn);
    cJSON_AddNumberToObject(cell_obj, "prb", cells[i].cell.nof_prb);
    cJSON_AddNumberToObject(cell_obj, "ports", cells[i].cell.nof_ports);
    cJSON_AddNumberToObject(cell_obj, "pss_power", srsran_convert_power_to_dB(cells[i].power));

    cJSON_AddItemToArray(cell_array, cell_obj);
  }

  cJSON_AddItemToObject(root, "cells", cell_array);

  char* json_str = cJSON_Print(root);
  if (!json_str) {
    printf("ERROR: Failed to create JSON string\n");
    exit(-1);
  }

  FILE* fp = fopen("cell_info.json", "w");
  if (fp) {
    fputs(json_str, fp);
    fclose(fp);
  } else {
    printf("ERROR: Could not write JSON file\n");
  }

  free(json_str);
  cJSON_Delete(root);
}

void usage(char* prog)
{
  printf("Usage: %s [agsendtvb] -b band\n", prog);
  printf("\t-a RF args [Default %s]\n", rf_args);
  printf("\t-d RF devicename [Default %s]\n", rf_dev);
  printf("\t-g RF gain [Default %.2f dB]\n", rf_gain);
  printf("\t-s earfcn_start [Default All]\n");
  printf("\t-e earfcn_end [Default All]\n");
  printf("\t-n nof_frames_total [Default 100]\n");
  printf("\t-v [set srsran_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "agsendvbt")) != -1) {
    switch (opt) {
      case 'a':
        rf_args = argv[optind];
        break;
      case 'b':
        band = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'd':
        rf_dev = argv[optind];
        break;
      case 's':
        earfcn_start = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'e':
        earfcn_end = (int)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        cell_detect_config.max_frames_pss = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'g':
        rf_gain = strtof(argv[optind], NULL);
        break;
      case 'v':
        increase_srsran_verbose_level();
        break;
        case 't':
        test_mode = true;
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (!test_mode && band == -1) 
  {
    usage(argv[0]);
    exit(-1);
  }
}

int srsran_rf_recv_wrapper(void* h, void* data, uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ---- ", nsamples);
  return srsran_rf_recv_with_time((srsran_rf_t*)h, data, nsamples, 1, NULL, NULL);
}

bool go_exit = false;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  }
}

static SRSRAN_AGC_CALLBACK(srsran_rf_set_rx_gain_wrapper)
{
  srsran_rf_set_rx_gain((srsran_rf_t*)h, gain_db);
}

int main(int argc, char** argv)
{
  int                           n;
  srsran_rf_t                   rf;
  srsran_ue_cellsearch_t        cs;
  srsran_ue_cellsearch_result_t found_cells[3];
  int                           nof_freqs;
  srsran_earfcn_t               channels[MAX_EARFCN];
  uint32_t                      freq;
  uint32_t                      n_found_cells = 0;

  srsran_debug_handle_crash(argc, argv);

  parse_args(argc, argv);

  if (test_mode) {
    printf("Running in TEST MODE. Skipping RF init.\n");
  
    n_found_cells = 2;
  
    results[0].cell.id = 100;
    results[0].cell.nof_prb = 50;
    results[0].cell.nof_ports = 2;
    results[0].freq = 2685.0;
    results[0].dl_earfcn = 18000;
    results[0].power = 0.05;
  
    results[1].cell.id = 101;
    results[1].cell.nof_prb = 75;
    results[1].cell.nof_ports = 4;
    results[1].freq = 2650.0;
    results[1].dl_earfcn = 18500;
    results[1].power = 0.1;
  
    goto output_results;
  }


  printf("Opening RF device...\n");

  if (srsran_rf_open_devname(&rf, rf_dev, rf_args, 1)) {
    ERROR("Error opening rf");
    exit(-1);
  }
  if (!cell_detect_config.init_agc) {
    srsran_rf_set_rx_gain(&rf, rf_gain);
  } else {
    printf("Starting AGC thread...\n");
    if (srsran_rf_start_gain_thread(&rf, false)) {
      ERROR("Error opening rf");
      exit(-1);
    }
    srsran_rf_set_rx_gain(&rf, 50);
  }

  // Supress RF messages
  srsran_rf_suppress_stdout(&rf);

  nof_freqs = srsran_band_get_fd_band(band, channels, earfcn_start, earfcn_end, MAX_EARFCN);
  if (nof_freqs < 0) {
    ERROR("Error getting EARFCN list");
    exit(-1);
  }

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  signal(SIGINT, sig_int_handler);

  if (srsran_ue_cellsearch_init(&cs, cell_detect_config.max_frames_pss, srsran_rf_recv_wrapper, (void*)&rf)) {
    ERROR("Error initiating UE cell detect");
    exit(-1);
  }

  if (cell_detect_config.max_frames_pss) {
    srsran_ue_cellsearch_set_nof_valid_frames(&cs, cell_detect_config.nof_valid_pss_frames);
  }
  if (cell_detect_config.init_agc) {
    srsran_rf_info_t* rf_info = srsran_rf_get_info(&rf);
    srsran_ue_sync_start_agc(&cs.ue_sync,
                             srsran_rf_set_rx_gain_wrapper,
                             rf_info->min_rx_gain,
                             rf_info->max_rx_gain,
                             cell_detect_config.init_agc);
  }

  for (freq = 0; freq < nof_freqs && !go_exit; freq++) {
    /* set rf_freq */
    srsran_rf_set_rx_freq(&rf, 0, (double)channels[freq].fd * MHZ);
    INFO("Set rf_freq to %.3f MHz", (double)channels[freq].fd * MHZ / 1000000);

    printf(
        "[%3d/%d]: EARFCN %d Freq. %.2f MHz looking for PSS.\n", freq, nof_freqs, channels[freq].id, channels[freq].fd);
    fflush(stdout);

    if (SRSRAN_VERBOSE_ISINFO()) {
      printf("\n");
    }

    bzero(found_cells, 3 * sizeof(srsran_ue_cellsearch_result_t));

    INFO("Setting sampling frequency %.2f MHz for PSS search", SRSRAN_CS_SAMP_FREQ / 1000000);
    srsran_rf_set_rx_srate(&rf, SRSRAN_CS_SAMP_FREQ);
    INFO("Starting receiver...");
    srsran_rf_start_rx_stream(&rf, false);

    n = srsran_ue_cellsearch_scan(&cs, found_cells, NULL);
    if (n < 0) {
      ERROR("Error searching cell");
      exit(-1);
    } else if (n > 0) {
      for (int i = 0; i < 3; i++) {
        if (found_cells[i].psr > 2.0) {
          srsran_cell_t cell;
          cell.id = found_cells[i].cell_id;
          cell.cp = found_cells[i].cp;
          int ret = rf_mib_decoder(&rf, 1, &cell_detect_config, &cell, NULL);
          if (ret < 0) {
            ERROR("Error decoding MIB");
            exit(-1);
          }
          if (ret == SRSRAN_UE_MIB_FOUND) {
            printf("Found CELL ID %d. %d PRB, %d ports\n", cell.id, cell.nof_prb, cell.nof_ports);
            if (cell.nof_ports > 0) {
              results[n_found_cells].cell      = cell;
              results[n_found_cells].freq      = channels[freq].fd;
              results[n_found_cells].dl_earfcn = channels[freq].id;
              results[n_found_cells].power     = found_cells[i].peak;
              n_found_cells++;
            }
          }
        }
      }
    }
  }

output_results:
  printf("\n\nFound %d cells\n", n_found_cells);
  for (int i = 0; i < n_found_cells; i++) {
    printf("Found CELL %.1f MHz, EARFCN=%d, PHYID=%d, %d PRB, %d ports, PSS power=%.1f dBm\n",
           results[i].freq,
           results[i].dl_earfcn,
           results[i].cell.id,
           results[i].cell.nof_prb,
           results[i].cell.nof_ports,
           srsran_convert_power_to_dB(results[i].power));
          //  SerializeCell(&results[i]);
  }
  SerializeCells(results,n_found_cells);

  printf("\nBye\n");

  if(!test_mode)
{  
  srsran_ue_cellsearch_free(&cs);
  srsran_rf_close(&rf);
}
  exit(0);
}
