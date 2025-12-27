#include <raylib.h>
#include <stdlib.h>
#include <random>
#include <bitset>
#include <ctime>
#include <iostream>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "VminiRV.h"
#include "gm.cpp"

#include <fstream>
#include <vector>
#include <stdexcept>
#include <algorithm>

struct Tester_gm_dut {
  bool is_diff = false;
  miniRV* gm;
  VminiRV* dut;
  GmVcdTrace* gm_trace;
  VerilatedVcdC* m_trace;

  inst_size_t* insts;
  uint32_t n_insts;

  uint64_t max_sim_time;
  uint64_t cycle = 0;

  uint8_t* dpi_c_memory;
  uint8_t* dpi_c_vga;
};

static inline std::string trim(std::string s) {
  auto notSpace = [](unsigned char c){ return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}

std::vector<std::uint8_t> read_hex_bytes_one_per_line(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("Cannot open file: " + path);

  std::vector<std::uint8_t> bytes;
  std::string line;
  std::size_t lineNo = 0;

  while (std::getline(in, line)) {
    ++lineNo;

    // Strip inline comments starting with '#' or ';' or '//'
    auto cut = line.find('#');
    if (cut != std::string::npos) line.erase(cut);
    cut = line.find(';');
    if (cut != std::string::npos) line.erase(cut);
    cut = line.find("//");
    if (cut != std::string::npos) line.erase(cut);

    line = trim(line);
    if (line.empty()) continue;

    // Optional 0x / 0X prefix
    if (line.size() >= 2 && line[0] == '0' && (line[1] == 'x' || line[1] == 'X')) {
      line = line.substr(2);
      line = trim(line);
    }

    // Expect 1 or 2 hex digits for a byte (e.g., "A" or "0A" or "ff")
    if (line.size() == 1) line = "0" + line;
    if (line.size() != 2)
      throw std::runtime_error("Invalid byte on line " + std::to_string(lineNo) + ": '" + line + "'");

    // Parse as hex
    std::size_t idx = 0;
    unsigned long val = 0;
    try {
      val = std::stoul(line, &idx, 16);
    } catch (...) {
      throw std::runtime_error("Non-hex data on line " + std::to_string(lineNo) + ": '" + line + "'");
    }
    if (idx != line.size() || val > 0xFF)
      throw std::runtime_error("Out-of-range byte on line " + std::to_string(lineNo) + ": '" + line + "'");

    bytes.push_back(static_cast<std::uint8_t>(val));
  }

  return bytes;
}

std::vector<uint8_t> read_bin_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  std::vector<uint8_t> buffer;

  if (!file) {
    std::cerr << "Error: Could not open " << path << "\n";
    return buffer;
  }

  const std::streamsize size = file.tellg();
  if (size <= 0) {               // covers empty file (0) and tellg() failure (-1)
    return buffer;
  }

  buffer.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);

  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    std::cerr << "Error: Could not read file.\n";
    buffer.clear();
  }

  return buffer;
}

void clock_tick(miniRV* cpu) {
  cpu->clock.prev = cpu->clock.curr;
  cpu->clock.curr ^= 1;
}

void dump_trace(Tester_gm_dut* tester) {
  tester->m_trace->dump(tester->cycle);
}

void dut_cycle(Tester_gm_dut* tester) {
  tester->dut->eval();
  dump_trace(tester);
  tester->cycle++;
  tester->dut->clock ^= 1;

  tester->dut->eval();
  dump_trace(tester);
  tester->cycle++;
  tester->dut->clock ^= 1;
}

void dut_wait(Tester_gm_dut* tester) {
  dut_cycle(tester);
  while (!tester->dut->is_step_finished) {
    dut_cycle(tester);
  }
}

void dut_reset(Tester_gm_dut* tester) {
  tester->dut->reset = 1;
  dut_cycle(tester);
  tester->dut->reset = 0;
  tester->dut->clock = 0;
}
void reset_gm_regs(miniRV* gm) {
  cpu_reset_regs(gm);
  gm->clock.prev = 0;
  gm->clock.curr = 0;
  gm->reset.prev = 0;
  gm->reset.curr = 0;
}

void dut_ram_write(uint32_t addr, uint32_t wdata, uint8_t wstrb) {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  VminiRV::sv_mem_write(addr, wdata, wstrb);
}

uint32_t dut_ram_read(uint32_t addr) {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  return VminiRV::sv_mem_read(addr);
}

uint8_t* dut_ram_ptr() {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  return (uint8_t*)VminiRV::sv_mem_ptr();
}

uint8_t* dut_vga_ptr() {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  return (uint8_t*)VminiRV::sv_vga_ptr();
}

uint8_t* dut_uart_ptr() {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  return (uint8_t*)VminiRV::sv_uart_ptr();
}

uint32_t* dut_time_ptr() {
  svScope ram_scope = svGetScopeFromName("TOP.miniRV.u_ram");
  if (!ram_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_ram\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(ram_scope);
  return (uint32_t*)VminiRV::sv_time_ptr();
}

bool compare_reg(uint64_t sim_time, const char* name, uint32_t dut_v, uint32_t gm_v) {
  if (dut_v != gm_v) {
    printf("Test Failed at time %lu. %s mismatch: dut_v = 0x%x vs gm_v = 0x%x\n", sim_time, name, dut_v, gm_v);
    return false;
  }
  return true;
}

bool compare_mem(uint64_t sim_time, uint32_t address, uint32_t dut_v, uint32_t gm_v) {
  if (dut_v != gm_v) {
    printf("Test Failed at time %lu. %x mismatch: dut_v = %i vs gm_v = %i\n", sim_time, address, dut_v, gm_v);
    return false;
  }
  return true;
}

bool compare(Tester_gm_dut* tester, uint64_t sim_time) {
  bool result = true;
  result &= compare_reg(sim_time, "EBREAK", tester->dut->ebreak, tester->gm->ebreak.v);
  result &= compare_reg(sim_time, "PC", tester->dut->pc, tester->gm->pc.v);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'R', digit1, digit0, '\0'};
    result &= compare_reg(sim_time, name, tester->dut->regs[i], tester->gm->regs[i].v);
  }
  result &= memcmp(tester->gm->vga, tester->dpi_c_vga, VGA_SIZE) == 0;
  result &= memcmp(tester->gm->mem, tester->dpi_c_memory, MEM_SIZE) == 0;
  if (!result) {
    for (uint32_t i = 0; i < MEM_SIZE; i++) {
      uint32_t dut_v = dut_ram_read(i);
      uint32_t gm_v = gm_mem_read(tester->gm, {i}).v;
      result &= compare_mem(sim_time, i, dut_v, gm_v);
    }
    for (uint32_t i = VGA_START; i < VGA_END; i++) {
      uint32_t dut_v = dut_ram_read(i);
      uint32_t gm_v = gm_mem_read(tester->gm, {i}).v;
      result &= compare_mem(sim_time, i, dut_v, gm_v);
    }
  }
  return result;
}

uint64_t hash_uint64_t(uint64_t x) {
  x *= 0xff51afd7ed558ccd;
  x ^= x >> 32;
  return x;
}

uint32_t random_range(std::mt19937* gen, uint32_t ge, uint32_t lt) {
  std::uniform_int_distribution<uint32_t> dist(0, (1U << lt) - 1);

  return dist(*gen) % lt + ge;
}

uint32_t random_bits(std::mt19937* gen, uint32_t n) {
  std::uniform_int_distribution<uint32_t> dist(0, (1U << n) - 1);
  return dist(*gen);
}

inst_size_t random_instruction(std::mt19937* gen) {
  uint32_t inst_id = random_range(gen, 0, 11); // NOTE: don't test ebreak right now
  inst_size_t inst = {};
  switch (inst_id) {
    case 0: { // ADDI
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = addi(imm, rs1, rd);
    } break;
    case 1: { // JALR
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = addi(imm, rs1, rd);
      // inst = jalr(imm, rs1, rd);
    } break;
    case 2: { // ADD
      uint32_t rs2 = random_bits(gen, 4);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = add(rs2, rs1, rd);
    } break;
    case 3: { // LUI
      uint32_t imm = random_bits(gen, 20);
      uint32_t rd  = random_bits(gen, 4);
      inst = lui(imm, rd);
    } break;
    case 4: { // LW
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = lw(imm, rs1, rd);
    } break;
    case 5: { // LB
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = lbu(imm, rs1, rd);
    } break;
    case 6: { // SW
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs2 = random_bits(gen, 4);
      uint32_t rs1  = random_bits(gen, 4);
      inst = sw(imm, rs2, rs1);
    } break;
    case 7: { // SB
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs2 = random_bits(gen, 4);
      uint32_t rs1  = random_bits(gen, 4);
      inst = sb(imm, rs2, rs1);
    } break;
    case 8: {
      uint32_t imm = random_bits(gen, 5);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = slli(imm, rs1, rd);
    } break;
    case 9: {
      uint32_t imm = random_bits(gen, 5);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = srli(imm, rs1, rd);
    } break;
    case 10: {
      uint32_t imm = random_bits(gen, 5);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = srai(imm, rs1, rd);
    } break;
    case 100: { // EBREAK
      inst = ebreak();
    } break;
  }
  return inst;
}

inst_size_t random_instruction_mem_load_or_store(std::mt19937* gen) {
  uint32_t inst_id = random_range(gen, 0, 4);
  inst_size_t inst = {};
  switch (inst_id) {
    case 0: { // LW
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = lw(imm, rs1, rd);
    } break;
    case 1: { // LB
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs1 = random_bits(gen, 4);
      uint32_t rd  = random_bits(gen, 4);
      inst = lbu(imm, rs1, rd);
    } break;
    case 2: { // SW
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs2 = random_bits(gen, 4);
      uint32_t rs1  = random_bits(gen, 4);
      inst = sw(imm, rs2, rs1);
    } break;
    case 3: { // SB
      uint32_t imm = random_bits(gen, 12);
      uint32_t rs2 = random_bits(gen, 4);
      uint32_t rs1  = random_bits(gen, 4);
      inst = sb(imm, rs2, rs1);
    } break;
  }
  return inst;
}

Tester_gm_dut* new_tester() {
  Tester_gm_dut* result = new Tester_gm_dut;
  result->gm = new miniRV;
  result->dut = new VminiRV;
  Verilated::traceEverOn(true);
  result->m_trace = new VerilatedVcdC;
  result->dut->trace(result->m_trace, 5);

  // result->gm_trace = new GmVcdTrace(result->gm);
  // result->m_trace->spTrace()->addInitCb(&GmVcdTrace::init_cb, result->gm_trace);
  // result->m_trace->spTrace()->addFullCb(&GmVcdTrace::full_cb, 0, result->gm_trace);
  // result->m_trace->spTrace()->addChgCb (&GmVcdTrace::chg_cb,  0, result->gm_trace);
  result->m_trace->open("waveform.vcd");

  result->dpi_c_memory = dut_ram_ptr();
  result->dpi_c_vga = dut_vga_ptr();
  result->gm->uart_ref = dut_uart_ptr();
  result->gm->time_uptime_ref = dut_time_ptr();

  result->n_insts = 0;
  result->insts = nullptr;
  return result;
}

void delete_tester(Tester_gm_dut* tester) {
  tester->m_trace->close();
  if (tester->n_insts) {
    delete [] tester->insts;
  }
  delete tester->gm_trace;
  delete tester->m_trace;
  delete tester->dut;
  delete tester->gm;
  delete tester;
}

bool is_valid_pc_address(uint32_t pc, uint32_t n_insts) {
  uint32_t min_address = MEM_START;
  uint32_t max_address = 4*n_insts + MEM_START;
  return min_address <= pc && pc <= max_address;
}

bool test_instructions(Tester_gm_dut* tester) {
  bool is_test_success = true;

  VminiRV* dut = tester->dut;
  miniRV* gm  = tester->gm;

  dut_reset(tester);
  if (tester->is_diff) {
    reset_gm_regs(gm);
    gm_mem_reset(gm);
  }

  dut->clock = 0;
  dut->top_mem_wen = 1;
  for (uint32_t i = 0; i < tester->n_insts; i++) {
    uint32_t address = 4*i + MEM_START;
    dut->top_mem_wdata = tester->insts[i].v;
    dut->top_mem_addr = address;
    dut_wait(tester);

    if (tester->is_diff) {
      gm_mem_write(gm, {1}, {1, 1, 1, 1}, {address}, tester->insts[i]);
    }
    // print_instruction(tester->insts[i]);
  }
  dut->top_mem_wen = 0;
  if (tester->is_diff) {
    reset_gm_regs(gm);
  }
  dut->clock = 0;
  for (uint64_t t = 0; !tester->max_sim_time || t < tester->max_sim_time && is_valid_pc_address(gm->pc.v, tester->n_insts) && is_valid_pc_address(dut->pc, tester->n_insts); t++) {
    dut_wait(tester);
    if (dut->ebreak) {
      printf("dut ebreak\n");
      if (dut->regs[10] != 0) {
        printf("test is not successful: dut code %u\n", dut->regs[10]);
        is_test_success=false;
      }
    }

    if (tester->is_diff) {
      inst_size_t inst = gm_mem_read(gm, gm->pc);
      CPU_out out = cpu_eval(gm);
      if (gm->ebreak.v) {
        printf("gm ebreak\n");
        if (gm->regs[10].v != 0) {
          printf("test is not successful: gm code %u\n", gm->regs[10].v);
          is_test_success=false;
        }
      }
      is_test_success &= compare(tester, t);
      if (!is_test_success) {
        // for (uint32_t i = 0; i < tester->n_insts; i++) {
        //   printf("pc=%x: ", 4*i);
        //   print_instruction(tester->insts[i]);
        // }
        printf("[%x] pc=%x inst:", t, gm->pc.v);
        print_instruction(inst);
        break;
      }
    }

    if (tester->is_diff) {
      if (dut->ebreak && gm->ebreak.v) break;
    }
    else if (dut->ebreak) {
      printf("finished:%u\n", tester->cycle);
      break;
    }
  }
  // dut_reset(tester);
  if (tester->is_diff) {
    gm_mem_reset(gm);
    reset_gm_regs(gm);
  }

  return is_test_success;
}

void print_all_instructions(Tester_gm_dut* tester) {
  for (uint32_t i = 0; i < tester->n_insts; i++) {
    print_instruction(tester->insts[i]);
  }
}

bool random_difftest(Tester_gm_dut* tester) {
  tester->n_insts = 10;
  tester->insts = new inst_size_t[tester->n_insts];
  bool is_tests_success = true;
  uint64_t tests_passed = 0;
  tester->max_sim_time = 5000;
  uint64_t max_tests = 2;
  // uint64_t seed = hash_uint64_t(std::time(0));
  uint64_t seed = 5578876383785782017lu;
  uint64_t i_test = 0;
  do {
    printf("======== SEED:%lu ===== %u/%u =========\n", seed, i_test, max_tests);
    std::random_device rd;
    std::mt19937 gen(rd());
    gen.seed(seed);
    for (uint32_t i = 0; i < tester->n_insts; i++) {
      // inst_size_t inst = random_instruction_mem_load_or_store(&gen);
      inst_size_t inst = random_instruction(&gen);
      tester->insts[i] = inst;
    }
    // tester->insts[0] = lui(0x80010, 2);
    // tester->insts[1] = li(47, 3);
    // tester->insts[2] = sw(0, 3, 2);
    // tester->insts[3] = lw(0, 2, 4);
    // tester->n_insts = 4;

    is_tests_success &= test_instructions(tester);
    if (is_tests_success) {
      tests_passed++;
    }
    else {
      print_all_instructions(tester);
    }
    seed = hash_uint64_t(seed);
    i_test++;
  } while (is_tests_success && tests_passed < max_tests);

  std::cout << "Tests results:\n" << tests_passed << " / " << max_tests << " have passed\n";
  return is_tests_success;
}

void draw_image_raylib(uint8_t* image, uint32_t width, uint32_t height) {
  auto *fb = reinterpret_cast<void*>(image);

  InitWindow(width, height, "Framebuffer viewer");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);

    // Scale it up to the window
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint32_t p = ((uint32_t*)fb)[y*height + x];
        unsigned char blue = p & 0xff;
        unsigned char green = (p >> 8) & 0xff;
        unsigned char red = (p >> 16) & 0xff;
        Color c = { red, green, blue, 255};
        DrawPixel(x, y, c);
      }
    }
    EndDrawing();
  }

  CloseWindow();
}

void vga_image_test() {
  VminiRV* dut = new VminiRV;
  const uint64_t base = 0x20000000;
  const uint32_t width  = 256;
  const uint32_t height = 256;
  const uint64_t nbytes = width*height*4;
  uint8_t* image = new uint8_t[nbytes];
  auto hex_bytes = read_hex_bytes_one_per_line("vga2.hex");
  // dut_reset(dut);
  for (uint32_t i = 0; i < hex_bytes.size(); i++) {
    dut_ram_write(i, hex_bytes[i], 0b0001);
  }

  for (uint64_t t = 0; t < 2000000; t++) {
    uint32_t inst = dut_ram_read(dut->pc);
    dut->eval();
    dut->clock ^= 1;
  }

  for (uint64_t i = 0; i < nbytes; i++) {
    image[i] = dut_ram_read(i+base);
  }
  draw_image_raylib(image, width, height);
}

void vga_image_gm() {
  miniRV* gm = new miniRV;
  const uint32_t base = 0x20000000;
  const uint32_t width  = 256;
  const uint32_t height = 256;
  const uint32_t nbytes = width*height*4;
  uint8_t* image = new uint8_t[nbytes];
  auto hex_bytes = read_hex_bytes_one_per_line("vga2.hex");
  gm_mem_reset(gm);
  for (uint32_t i = 0; i < hex_bytes.size(); i++) {
    gm->mem[i].v = hex_bytes[i];
  }
  reset_gm_regs(gm);
  for (uint32_t t = 0; t < 2000000; t++) {
    cpu_eval(gm);
    clock_tick(gm);
  }
  for (uint32_t i = 0; i < nbytes; i++) {
    image[i] = gm_mem_read(gm, {i+base}).v;
  }
  draw_image_raylib(image, width, height);
}

bool bin_test(Tester_gm_dut* tester, const std::string& path) {
  std::vector<uint8_t> buffer = read_bin_file(path);
  tester->max_sim_time = 0;
  tester->n_insts = buffer.size() / 4;
  tester->insts = new inst_size_t[tester->n_insts];
  for (uint32_t i = 0; i < tester->n_insts; i++) {
    uint32_t byte3 = buffer[4*i + 3] << 24;
    uint32_t byte2 = buffer[4*i + 2] << 16;
    uint32_t byte1 = buffer[4*i + 1] <<  8;
    uint32_t byte0 = buffer[4*i + 0] <<  0;
    tester->insts[i] = { byte0 | byte1 | byte2 | byte3 };
    // print_instruction(tester->insts[i]);
  }
  return test_instructions(tester);
}

static void usage(const char* prog) {
  fprintf(stderr,
    "Usage:\n"
    "  %s [diff] random\n"
    "  %s [diff] bin <path>\n",
    prog, prog
  );
}

static int streq(const char* a, const char* b) {
  return a && b && strcmp(a, b) == 0;
}

void game(Tester_gm_dut* tester) {
  const uint32_t width  = 256;
  const uint32_t height = 256;
  const uint32_t nbytes = width*height*4;
  uint8_t* image = new uint8_t[nbytes];
  auto buffer = read_bin_file("game-minirv-npc.bin");
  tester->max_sim_time = 0;
  tester->n_insts = buffer.size() / 4;
  tester->insts = new inst_size_t[tester->n_insts];
  for (uint32_t i = 0; i < tester->n_insts; i++) {
    uint32_t byte3 = buffer[4*i + 3] << 24;
    uint32_t byte2 = buffer[4*i + 2] << 16;
    uint32_t byte1 = buffer[4*i + 1] <<  8;
    uint32_t byte0 = buffer[4*i + 0] <<  0;
    tester->insts[i] = { byte0 | byte1 | byte2 | byte3 };
    // print_instruction(tester->insts[i]);
  }
  tester->dut->clock = 0;
  tester->dut->top_mem_wen = 1;
  for (uint32_t i = 0; i < tester->n_insts; i++) {
    uint32_t address = 4*i + MEM_START;
    tester->dut->top_mem_wdata = tester->insts[i].v;
    tester->dut->top_mem_addr = address;
    tester->dut->clock = 0;
    tester->dut->eval();
    tester->dut->clock = 1;
    tester->dut->eval();
    // print_instruction(tester->insts[i]);
  }
  tester->dut->top_mem_wen = 0;

  InitWindow(width, height, "Framebuffer viewer");
  SetTargetFPS(60);

  while (!WindowShouldClose() && !tester->dut->ebreak) {
    BeginDrawing();
    ClearBackground(BLACK);

    {
      uint32_t key = GetKeyPressed();
      // printf("key: %u\n", key);
      tester->dut->top_mem_wen = 1;
      tester->dut->top_mem_addr  = UART_DATA_ADDR;
      tester->dut->top_mem_wdata = key;
      tester->dut->clock = 0;
      tester->dut->eval();
      tester->dut->clock = 1;
      tester->dut->eval();
      tester->dut->top_mem_wen = 0;
    }

    // ONE CYCLE
    for (uint32_t i = 0; i < 1; i++) {
      tester->dut->eval();
      tester->dut->clock ^= 1;
      tester->dut->eval();
      tester->dut->clock ^= 1;
    }

    // ONE FRAME
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint32_t p = ((uint32_t*)tester->dpi_c_vga)[y*height + x];
        unsigned char blue = p & 0xff;
        unsigned char green = (p >> 8) & 0xff;
        unsigned char red = (p >> 16) & 0xff;
        Color c = { red, green, blue, 255};
        DrawPixel(x, y, c);
      }
    }
    EndDrawing();
  }

  CloseWindow();


  for (uint32_t i = 0; i < nbytes; i++) {
    image[i] = dut_ram_read(i+VGA_START);
    printf("%u\n", image[i]);
  }
  draw_image_raylib(image, width, height);
}

int main(int argc, char** argv, char** env) {
  Tester_gm_dut* tester = new_tester();
  if (!tester) {
    fprintf(stderr, "Error: failed to create tester\n");
    return EXIT_FAILURE;
  }

  int exit_code = EXIT_SUCCESS;

  if (argc < 2) {
    usage(argv[0]);
    exit_code = EXIT_FAILURE;
    goto cleanup;
  }
  else {
    char* mode = argv[1];

    if (streq(mode, "diff")) {
      if (argc <= 2) {
        fprintf(stderr, "Error: 'diff' requires other modes to run\n");
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
        goto cleanup;
      }
      tester->is_diff = true;
      mode = argv[2];
    }

    if (streq(mode, "random")) {
      if (argc != 2 + tester->is_diff) {
        fprintf(stderr, "Error: 'random' takes no extra arguments\n");
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
        goto cleanup;
      }
      if (!random_difftest(tester)) exit_code = EXIT_FAILURE;

    }
    else if (streq(mode, "bin")) {
      if (argc < 3 + tester->is_diff) {
        fprintf(stderr, "Error: 'bin' requires a <path>\n");
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
        goto cleanup;
      }
      if (argc != 3 + tester->is_diff) {
        fprintf(stderr, "Error: too many arguments for 'bin'\n");
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
        goto cleanup;
      }
      const char* path = argv[2 + tester->is_diff];
      printf("%u, %s\n", argc, path);
      if (!bin_test(tester, path)) exit_code = EXIT_FAILURE;
    }
    else if (streq(mode, "game")) {
      if (argc != 2 + tester->is_diff) {
        fprintf(stderr, "Error: too many arguments for 'game'\n");
        usage(argv[0]);
        exit_code = EXIT_FAILURE;
        goto cleanup;
      }
      game(tester);
    }
    else if (streq(mode, "-h") || streq(mode, "--help") || streq(mode, "help")) {
      usage(argv[0]);

    }
    else {
      fprintf(stderr, "Error: unknown mode '%s'\n", mode);
      usage(argv[0]);
      exit_code = EXIT_FAILURE;
    }
  }

cleanup:
  delete_tester(tester);
  return exit_code;
}


