#ifndef UMM_UM_PG_TBL_MGR
#define UMM_UM_PG_TBL_MGR

#include "stdint.h"

#define printf ebbrt::kprintf_force

#define SMALL_PG_SHIFT 12
#define MED_PG_SHIFT 21
#define LG_PG_SHIFT 30

// Order for page allocator. 0, 9, 18.
extern const unsigned char orders[];

// Size of shift for various pages, 12, 23, 30.
extern const unsigned char pgShifts[];

// Number of bytes for various page sizes.
extern const uint64_t pgBytes[];

// Indexed names for debugging printers..
extern const char* level_names[];

// Magic PML4 entry chosen for slot.
#define SLOT_PML4_NUM 0x180

// For the page allocator.
enum Orders {
  SMALL_ORDER = 0,
  MEDIUM_ORDER = 9,
  LARGE_ORDER = 18
};

enum Level {
  PML4_LEVEL = 4,
  PDPT_LEVEL = 3,
  DIR_LEVEL = 2,
  TBL_LEVEL = 1,
  NO_LEVEL = 0
};

enum PgSz {
  _1G__ = 3,
  _2M__ = 2,
  _4K__ = 1
};

namespace umm {

  // TODO(tommyu):where should this go?
class lin_addr;
// Simplified page table entry.
class simple_pte {
 public:
  union {
    uint64_t raw;
    struct {
    //   // Ok to grab upper 40 for PN b/c (top reserved 0).
      uint64_t
      WHOCARES : 12,
        PAGE_NUMBER : 40;
    } decomp4K;
    struct {
      uint64_t
      WHOCARES : 21,
        PAGE_NUMBER : 31;
    } decomp2M;
    struct {
      uint64_t
      WHOCARES : 30,
        PAGE_NUMBER : 22;
    } decomp1G;
    struct{
      uint64_t
        SEL : 1,
        RW : 1,
        US : 1,
        PWT: 1,
        PCD: 1,
        A : 1,
        DIRTY: 1,
        MAPS : 1,
        WHOCARES2 : 4,
        PG_TBL_ADDR : 40,
        RES : 12;
    } decompCommon;
  };
  uint64_t pageTabEntToPFN(unsigned char lvl);
  lin_addr pageTabEntToAddr(unsigned char lvl);
  lin_addr cr3ToAddr();
  void printCommon();
  void tableOrFramePtrToPte(simple_pte *tab);
  void clearPTE();
private:
  void printBits(uint64_t val, int len);
  void underlineNibbles();
  void printNibblesHex();
};
static_assert(sizeof(simple_pte) == sizeof(uint64_t), "Bad simple_pte SZ");

// Linear address with different interpretations.
class lin_addr {
public:
  union {
    uint64_t raw;

    struct {
      uint64_t OFFSET : 12, TAB : 9, DIR : 9, PDPT : 9, PML4 : 9;
    } tblOffsets;

    // TODO(tommyu): Dropping these here in removing phys_addr. Might be totally redundant with those below, need to check.
    struct {
      uint64_t OFFSET : 12, FRAME_NUMBER : 40;
    } construct_addr_4K;
    struct {
      uint64_t OFFSET : 21, FRAME_NUMBER : 27;
    } construct_addr_2M;
    struct {
      uint64_t OFFSET : 30, FRAME_NUMBER : 18;
    } construct_addr_1G;

    struct {
      // Ok to grab upper 40 for PN b/c (top reserved 0).
      uint64_t
      PGOFFSET : 12,
        PAGE_NUMBER : 40;
    } decomp4K;
    struct {
      uint64_t
      PGOFFSET : 21,
        PAGE_NUMBER : 31;
    } decomp2M;
    struct {
      uint64_t
      PGOFFSET : 30,
        PAGE_NUMBER : 22;
    } decomp1G;
  };
public:
  uint16_t operator[](uint8_t idx);
private:
  lin_addr getPhysAddrRec(simple_pte* root = nullptr, unsigned char lvl = 4);
  lin_addr getPhysAddrRecHelper(simple_pte* root, unsigned char lvl);
};
static_assert(sizeof(lin_addr) == sizeof(uint64_t), "Bad lin_addr SZ");

class UmPgTblMgr {
public:
  // NOTE: NYI. Higher level operations on page tables.
  static void areEqual();
  static void isSubset();
  static void difference();

  // Reclaimer
  static void reclaimAllPages(simple_pte *root, unsigned char lvl, bool reclaimPhysical = true);

  // Counters
  // TODO(tommyu): Do we want defaults? Maybe not.
  static void countDirtyPages(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countAccessedPages(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countValidPages(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countValidPTEs(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);

  static void traverseValidPages(simple_pte *root, uint8_t lvl);

  // Copiers
  static simple_pte * walkPgTblCopyDirty(simple_pte *root, simple_pte *copy = nullptr);
  static simple_pte * walkPgTblCopyDirty(simple_pte *root, simple_pte *copy, uint8_t lvl);

  // Extractors
  simple_pte *addrToPTE(lin_addr la, simple_pte *root = nullptr,
                        unsigned char lvl = 4);
  // Chasers
  static lin_addr getPhysAddrRec(lin_addr la, simple_pte *root = nullptr,
                           unsigned char lvl = 4);

  // Mappers
  static simple_pte *mapIntoPgTbl(simple_pte *root, lin_addr phys,
                                        lin_addr virt, unsigned char rootLvl,
                                        unsigned char mapLvl, unsigned char curLvl);

  // Root Getters
  static simple_pte *getSlotPDPTRoot();
  static simple_pte *getPML4Root();

  // Printers / DBers.
  // TODO(tommyu): Fix these names.
  static void dumpTableAddrs(simple_pte *root, unsigned char lvl);
  static void dumpFullTableAddrs(simple_pte *root, unsigned char lvl);

  // NOTE: World of lambdas begins here.
  static void printTraversalLamb(simple_pte *root, uint8_t lvl);

  static void countValidPagesLamb(std::vector<uint64_t> &counts,
                                  simple_pte *root, uint8_t lvl);

  static void countAccessedPagesLamb(std::vector<uint64_t> &counts,
                                  simple_pte *root, uint8_t lvl);

  static void countDirtyPagesLamb(std::vector<uint64_t> &counts,
                                  simple_pte *root, uint8_t lvl);

  template <typename LeafFn>
  static void traverseAccessedPages(simple_pte *root, uint8_t lvl, LeafFn L) {
    auto pred = [](simple_pte *curPte, uint8_t lvl) -> bool {
      // exists() is redundant assuming non existant ptes are 0.
      return exists(curPte) && isAccessed(curPte);
    };

    auto nullFn = [](simple_pte *curPte, uint8_t lvl){};
    auto nullAftRecFn = [](simple_pte *childRoot, simple_pte *curPte, uint8_t lvl){};

    traversePageTable(root, lvl, pred, nullFn, nullAftRecFn, L, nullFn);
  }

  template <typename LeafFn>
  static void traverseValidPages(simple_pte *root, uint8_t lvl, LeafFn L) {
    // Only takes a leaf function, rest are null.
    auto nullFn = [](simple_pte *curPte, uint8_t lvl){};
    auto nullAftRecFn = [](simple_pte *childRoot, simple_pte *curPte, uint8_t lvl){};
    traverseValidPages(root, lvl, nullFn, nullAftRecFn, L, nullFn);
  }

  template <typename BefRecFn, typename AftRecFn, typename LeafFn, typename BefRetFn>
  static void traverseValidPages(simple_pte *root, uint8_t lvl,
                                 BefRecFn BR, AftRecFn AR, LeafFn L, BefRetFn BRET) {
    auto pred = [](simple_pte *curPte, uint8_t lvl) -> bool {
      return exists(curPte);
    };
    traversePageTable(root, lvl, pred, BR, AR, L, BRET);
  }

  // Continue predicate, pre recursion, leaf, before return.
  // TODO(tommyu): Spell out full name.
  // TODO(tommyu): template too general, name types of lambdas.
  template <typename PredFn, typename BefRecFn, typename AftRecFn,
            typename LeafFn, typename BefRetFn>
static simple_pte *traversePageTable(simple_pte *root, uint8_t lvl,
                                     PredFn P, BefRecFn BR, AftRecFn AR,
                                     LeafFn L, BefRetFn BRET) {
    // A general page table traverser. Intention is not to call this directly
    // from client code, but to make specializations, like a fn that only walks
    // valid pages, accessed pages, dirty pages ...
    for (int i = 0; i < 512; i++) { // Loop over all entries in table.
      simple_pte *curPte = root + i;

      if (!P(curPte, lvl)) // ! allows pred to stay in positive sense for caller.
        continue;

      if (isLeaf(curPte, lvl)) { // -> a physical page of some sz.
        L(curPte, lvl);

      } else { // This entry points to a sub page table.
        BR(curPte, lvl);
        simple_pte *childRoot = traversePageTable(nextTableOrFrame(root, i, lvl), lvl - 1, P, BR, AR, L, BRET);
        // NOTE: This guy takes an extra arg.
        AR(childRoot, curPte, lvl);
      }
  }
  // NOTE: This guy takes root, not curPte!
  BRET(root, lvl);
  return root;
}

  static bool exists (simple_pte *pte);

private:
  UmPgTblMgr(); // Don't instantiate.

  // Mapper.
  static simple_pte *mapIntoPgTblHelper(simple_pte *root, lin_addr phys,
                                        lin_addr virt, unsigned char rootLvl,
                                        unsigned char mapLvl, unsigned char curLvl);


  // Counter Helpers
  static void countDirtyPagesHelper(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countAccessedPagesHelper(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countValidPagesHelper(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);
  static void countValidPTEsHelper(std::vector<uint64_t> &counts, simple_pte *root = nullptr, uint8_t lvl = PML4_LEVEL);

  // static void traverseValidPagesHelper(simple_pte *root, uint8_t);
  // Printers Debuggers
  static void dumpFullTableAddrsHelper(simple_pte *root, unsigned char lvl);

  static lin_addr copyDirtyPage(lin_addr src, unsigned char lvl);
  static lin_addr reconstructLinAddrPgFromOffsets(uint64_t *idx);
  simple_pte *aAddrToPTEHelper(lin_addr la, uint64_t *offsets, simple_pte *root,
                               unsigned char lvl);
  lin_addr cr3ToAddr();
  static simple_pte *nextTableOrFrame(simple_pte *pg_tbl_start, uint64_t pg_tbl_offset,
                               unsigned char lvl);
  static simple_pte *walkPgTblCopyDirtyHelper(simple_pte *root,
                                              simple_pte *copy,
                                              unsigned char lvl,
                                              uint64_t *idx);

  static lin_addr getPhysAddrRecHelper(lin_addr la, simple_pte *root, unsigned char lvl);
  static bool isLeaf     (simple_pte *pte, unsigned char lvl);
  static bool isAccessed (simple_pte *pte);
  static bool isDirty    (simple_pte *pte);
  static bool isReadOnly (simple_pte *pte);
  static bool isWritable (simple_pte *pte);
};

} // namespace umm

#endif //UMM_UM_PG_TBL_MGR
