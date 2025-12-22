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

void clock_tick(miniRV* cpu) {
  cpu->clock.prev = cpu->clock.curr;
  cpu->clock.curr ^= 1;
}

void reset_dut_regs(VminiRV* dut) {
  dut->reg_reset = 1;
  dut->eval();
  dut->reg_reset = 0;
  dut->clk = 0;
}

void reset_dut_mem(VminiRV* dut) {
  dut->mem_reset = 1;
  dut->eval();
  dut->mem_reset = 0;
  dut->clk = 0;
}

void reset_gm_regs(miniRV* gm) {
  cpu_reset_regs(gm);
  gm->clock.prev = 0;
  gm->clock.curr = 0;
  gm->reset.prev = 0;
  gm->reset.curr = 0;
}

void dut_ram_write(uint32_t addr, uint32_t wdata, uint8_t wstrb) {
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

void dut_rom_write(uint32_t addr, uint32_t wdata, uint8_t wstrb) {
  svScope rom_scope = svGetScopeFromName("TOP.miniRV.u_rom");
  if (!rom_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_rom\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(rom_scope);
  VminiRV::sv_rom_write(addr, wdata, wstrb);
}

uint32_t dut_rom_read(uint32_t addr) {
  svScope rom_scope = svGetScopeFromName("TOP.miniRV.u_rom");
  if (!rom_scope) {
    std::cerr << "ERROR: svGetScopeFromName(\"TOP.miniRV.u_rom\") returned NULL\n";
    std::exit(1);
  }
  svSetScope(rom_scope);
  return VminiRV::sv_rom_read(addr);
}

bool compare_reg(uint64_t sim_time, const char* name, uint32_t dut_v, uint32_t gm_v) {
  if (dut_v != gm_v) {
    printf("Test Failed at time %lu. %s mismatch: dut_v = %i vs gm_v = %i\n", sim_time, name, dut_v, gm_v);
    return false;
  }
  return true;
}

bool compare(VminiRV* dut, miniRV* gm, uint64_t sim_time) {
  bool result = true;
  result &= compare_reg(sim_time, "PC", dut->pc, gm->pc.v);
  for (uint32_t i = 0; i < N_REGS; i++) {
    char digit0 = i%10 + '0';
    char digit1 = i/10 + '0';
    char name[] = {'R', digit1, digit0, '\0'};
    result &= compare_reg(sim_time, name, dut->regs_out[i], gm->regs[i].v);
  }
  // for (uint32_t i = 0; i < MEM_SIZE; i++) {
  //   char digit0 = i%10 + '0';
  //   char digit1 = i/10 + '0';
  //   char name[] = {'M', digit1, digit0, '\0'};
  //   uint32_t dut_v = dut_ram_read(i);
  //   uint32_t gm_v = gm_mem_read(gm, {i}).v;
  //   result &= compare_reg(sim_time, name, dut_v, gm_v);
  // }
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
  uint32_t inst_id = random_range(gen, 0, 8);
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
      // inst = addi(imm, rs1, rd);
      inst = jalr(imm, rs1, rd);
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
  }
  return inst;
}

void random_difftest() {
  miniRV *gm = new miniRV;
  VminiRV *dut = new VminiRV;
  gm_mem_reset(gm);
  uint32_t n_insts = 200;
  inst_size_t* insts = new inst_size_t[n_insts];
  bool test_not_failed = true;
  uint64_t tests_passed = 0;
  uint64_t max_sim_time = 400;
  uint64_t max_tests = 1000;
  // uint64_t seed = hash_uint64_t(std::time(0));
  // uint64_t seed = 3263282379841580567;
  uint64_t seed = 10714955119269546755lu;
  do {

    printf("======== SEED:%lu ===== %u/%u =========\n", seed, tests_passed, max_tests);
    std::random_device rd;
    std::mt19937 gen(rd());
    gen.seed(seed);
    for (uint32_t i = 0; i < n_insts; i++) {
      inst_size_t inst = random_instruction(&gen);
      insts[i] = inst;
    }

    dut->clk = 0;
    dut->rom_wen = 1;
    for (uint32_t i = 0; i < n_insts; i++) {
      dut->rom_wdata = insts[i].v;
      dut->rom_addr = i*4;
      dut->clk = 0;
      dut->eval();
      dut->clk = 1;
      dut->eval();

      clock_tick(gm);
      gm_mem_write(gm, {1}, {1, 1, 1, 1}, {4*i}, insts[i]);
      clock_tick(gm);
    }
    dut->rom_wen = 0;
    reset_dut_regs(dut);
    reset_gm_regs(gm);
    dut->clk = 0;
    for (uint64_t t = 0; t < max_sim_time && gm->pc.v < n_insts*4; t++) {
      dut->eval();
      inst_size_t inst = gm_mem_read(gm, gm->pc);
      cpu_eval(gm);

      test_not_failed &= compare(dut, gm, t);

      if (!test_not_failed) {
        printf("[%u] %u inst: ", t, inst.v);
        print_instruction(inst);
        break;
      }

      clock_tick(gm);
      dut->clk ^= 1;
    }
    reset_dut_mem(dut);
    reset_dut_regs(dut);
    gm_mem_reset(gm);
    reset_gm_regs(gm);

    seed = hash_uint64_t(seed);
    if (test_not_failed) {
      tests_passed++;
    }
  } while (tests_passed < max_tests);


  std::cout << "Tests results:\n" << tests_passed << " / " << max_tests << " have passed\n";

  delete insts;
  delete dut;
  delete gm;
}

void draw_image_raylib(char* image, uint32_t width, uint32_t height) {
  auto *fb = reinterpret_cast<void*>(image);

  InitWindow(width, height, "Framebuffer viewer");
  SetTargetFPS(60);

  // Create a texture in the format you expect your buffer to be in.
  // Most common: RGBA8888 (bytes: R, G, B, A)
  Image img = {};
  img.data = fb; 
  img.width = width;
  img.height = height;
  img.mipmaps = 1;
  img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);

    // Scale it up to the window
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint32_t p = ((uint32_t*)fb)[y*256 + x];
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
  char* image = new char[nbytes];
  auto hex_bytes = read_hex_bytes_one_per_line("vga2.hex");
  reset_dut_mem(dut);
  reset_dut_regs(dut);
  for (uint32_t i = 0; i < hex_bytes.size(); i++) {
    dut_ram_write(i, hex_bytes[i], 0b0001);
  }

  for (uint64_t t = 0; t < 2000000; t++) {
    uint32_t inst = dut_ram_read(dut->pc);
    dut->eval();
    dut->clk ^= 1;
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
  char* image = new char[nbytes];
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

int main(int argc, char** argv, char** env) {
  random_difftest();
  // vga_image_test();
  // vga_image_gm();
  exit(EXIT_SUCCESS);
}


