// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

//#define NDIRECT 12 // edit
#define NDIRECT 11 // edit
#define NINDIRECT (BSIZE / sizeof(uint)) // 1024 deleno velkost zaznamu resp adresy co je 4B == 256 zaznamov/blokov
#define NDINDIRECT (NINDIRECT * NINDIRECT) // edit : number of double indirect zaznamov/blokov 256 * 256
//#define MAXFILE (NDIRECT + NINDIRECT) // edit
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT) // edit 11 + 256 + 256*256

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  //uint addrs[NDIRECT+1];   // Data block addresses //edit
  uint addrs[NDIRECT+2]; // Data block addresses // edit : 11 priamych jeden nepriamy a jeden double nepriamy
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

