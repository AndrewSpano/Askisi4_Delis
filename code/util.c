#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "util.h"



/* Extracts the n-th string from a buffer with n or more strings.
   Returns 1 if it succeds, else returns 0. */
int get_nth_string(char* str, const char buf[], int n)
{
  /* check for valid parameters */
  if (n <= 0)
  {
    return 0;
  }


  int i = 0;
  int j = 1;
  /* use the loop to skip the first n - 1 strings, while also checking
     for errors */
  for (; j <= n - 1; j++)
  {

    while (i < MAX_BUFFER_SIZE && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n' && buf[i] != '\0')
    {
      i++;
    }

    if (i == MAX_BUFFER_SIZE || (buf[i] != ' ' && buf[i] != '\t'))
    {
      return 0;
    }

    while (i < MAX_BUFFER_SIZE && (buf[i] == ' ' || buf[i] == '\t'))
    {
      i++;
    }

  }

  /* check if we have already iterated through the buffer */
  if (i == MAX_BUFFER_SIZE || (buf[i] == '\n' || buf[i] == '\0'))
  {
    return 0;
  }


  int k = 0;
  /* extract the n-th string */
  while (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n' && buf[i] != '\0')
  {
    str[k] = buf[i];
    i++;
    k++;

    if (i == 256)
    {
      memset(str, 0, k);
      return 0;
    }
  }
  str[k] = '\0';

  return 1;

}


/* If the buffer has a valid option, it returns the number of that option (1-14)
   Else, if there is an error, then -1 is returned */
int get_option(const char buffer[])
{
  /* string that contains the option */
  char option[MAX_BUFFER_SIZE];
  /* get the first string of the buffer to distinguish which option we have */
  int retval = get_nth_string(option, buffer, 1);

  if (retval == 0)
  {
    /* some error occured */
    return -1;
  }

  if (!strcmp(option, "cfs_workwith"))
  {
    return 1;
  }
  else if (!strcmp(option, "cfs_mkdir"))
  {
    return 2;
  }
  else if (!strcmp(option, "cfs_touch"))
  {
    return 3;
  }
  else if (!strcmp(option, "cfs_pwd"))
  {
    return 4;
  }
  else if (!strcmp(option, "cfs_cd"))
  {
    return 5;
  }
  else if (!strcmp(option, "cfs_ls"))
  {
    return 6;
  }
  else if (!strcmp(option, "cfs_cp"))
  {
    return 7;
  }
  else if (!strcmp(option, "cfs_cat"))
  {
    return 8;
  }
  else if (!strcmp(option, "cfs_ln"))
  {
    return 9;
  }
  else if (!strcmp(option, "cfs_mv"))
  {
    return 10;
  }
  else if (!strcmp(option, "cfs_rm"))
  {
    return 11;
  }
  else if (!strcmp(option, "cfs_import"))
  {
    return 12;
  }
  else if (!strcmp(option, "cfs_export"))
  {
    return 13;
  }
  else if (!strcmp(option, "cfs_create"))
  {
    return 14;
  }
  else
  {
    /* else no valid option has been found */
    return -1;
  }


}





/* ---------------------------------   NAVIGATORS   ----------------------------------------- */


/* function used to navigate through directory data blocks */
off_t* pointer_to_offset(char* pointer, size_t fns)
{
  char* name = pointer;
  name += fns;

  off_t* return_address = (off_t *) name;

  return return_address;
}


/* function used to navigate through directory data blocks */
char* pointer_to_next_name(char* pointer, size_t fns)
{
  off_t* offset = pointer_to_offset(pointer, fns);
  offset++;

  char* return_address = (char *) offset;

  return return_address;
}






/* ----------------------------  INITIALIZATION FUNCTIONS  --------------------------------- */


void initialize_superblock(superblock* my_superblock, char* cfs_filename, int fd, off_t root_directory_offset, size_t current_size, size_t bs, size_t fns, size_t cfs, uint mdfn)
{
  my_superblock->total_entities = 3;
  my_superblock->fd = fd;
  my_superblock->root_directory = root_directory_offset;
  my_superblock->current_size = current_size;
  my_superblock->block_size = bs;
  my_superblock->filename_size = fns;
  my_superblock->max_file_size = cfs;
  my_superblock->max_dir_file_number = mdfn;
  strcpy(my_superblock->cfs_filename, cfs_filename);
}



void initialize_holes(hole_map* holes, uint n, uint current_holes, off_t hole_start)
{
  holes->current_hole_number = current_holes;
  holes->holes_table[0].start = hole_start;
  holes->holes_table[0].end = 0;

  int i = 1;
  for (; i < n; i++)
  {
    holes->holes_table[i].start = 0;
    holes->holes_table[i].end = 0;
  }
}



void initialize_MDS(MDS* mds, uint id, uint type, uint number_of_hard_links, uint blocks_using, size_t size, off_t parent_offset, off_t first_block)
{
  mds->id = id;
  mds->type = type;
  mds->number_of_hard_links = number_of_hard_links;
  mds->blocks_using = blocks_using;

  mds->size = size;
  mds->parent_offset = parent_offset;
  mds->first_block = first_block;

  time_t my_time = time(NULL);
  mds->creation_time = my_time;
  mds->access_time = my_time;
  mds->modification_time = my_time;
}



void initialize_Directory_Data_Block(Block* block, size_t fns, off_t self_offset, off_t parent_offset)
{
  block->next_block = 0;

  char* name = (char *) block->data;
  off_t* offset = pointer_to_offset(name, fns);

  strcpy(name, "./");
  *offset = self_offset;

  name = pointer_to_next_name(name, fns);
  offset = pointer_to_offset(name, fns);

  strcpy(name, "../");
  *offset = parent_offset;

  /* we have currently stored 2 directories (./ and ../, and their offsets) */
  block->bytes_used = 2 * fns + 2 * sizeof(size_t);
}






/* --------------------------------  FAST ACCESS  --------------------------------------- */


superblock* get_superblock(int fd)
{
  superblock* my_superblock = NULL;

  MALLOC_OR_DIE_2(my_superblock, sizeof(superblock));

  LSEEK_OR_DIE_2(fd, 0, SEEK_SET, my_superblock);

  READ_OR_DIE(fd, my_superblock, sizeof(superblock));

  return my_superblock;
}


hole_map* get_hole_map(int fd)
{
  hole_map* holes = NULL;

  MALLOC_OR_DIE_2(holes, sizeof(hole_map));

  LSEEK_OR_DIE_2(fd, sizeof(superblock), SEEK_SET, holes);

  READ_OR_DIE(fd, holes, sizeof(hole_map));

  return holes;
}


MDS* get_MDS(int fd, off_t offset)
{
  MDS* mds = NULL;

  MALLOC_OR_DIE_2(mds, sizeof(MDS));

  LSEEK_OR_DIE_2(fd, offset, SEEK_SET, mds);

  READ_OR_DIE(fd, mds, sizeof(MDS));

  return mds;
}


Block* get_Block(int fd, size_t block_size, off_t offset)
{
  Block* block = NULL;

  MALLOC_OR_DIE_2(block, block_size);

  LSEEK_OR_DIE_2(fd, offset, SEEK_SET, block);

  READ_OR_DIE(fd, block, block_size);

  return block;
}





/* ----------------------------------  FAST SET  --------------------------------------- */


int set_superblock(superblock* superblock, int fd)
{
  DIE_IF_NULL(superblock);

  LSEEK_OR_DIE(fd, 0, SEEK_SET);

  WRITE_OR_DIE_2(fd, superblock, sizeof(superblock));

  return 1;
}


int set_hole_map(hole_map* holes, int fd)
{
  DIE_IF_NULL(holes);

  LSEEK_OR_DIE(fd, sizeof(superblock), SEEK_SET);

  WRITE_OR_DIE_2(fd, holes, sizeof(holes));

  return 1;
}


int set_MDS(MDS* mds, int fd, off_t offset)
{
  DIE_IF_NULL(mds);

  LSEEK_OR_DIE(fd, offset, SEEK_SET);

  WRITE_OR_DIE_2(fd, mds, sizeof(MDS));

  return 1;
}


int set_Block(Block* block, int fd, size_t block_size, off_t offset)
{
  DIE_IF_NULL(block);

  LSEEK_OR_DIE(fd, offset, SEEK_SET);

  WRITE_OR_DIE_2(fd, block, block_size);

  return 1;
}





/* --------------------------------  PRINT FUNCTIONS --------------------------------------- */


void print_superblock(superblock* my_superblock)
{
  printf("\n\nSUPERBLOCK\n\n");
  printf("fd = %d\n", my_superblock->fd);
  printf("cfs_filename = %s\n", my_superblock->cfs_filename);
  printf("total_entities = %u\n", my_superblock->total_entities);
  printf("root_directory = %lu\n", my_superblock->root_directory);
  printf("current_size = %lu\n", my_superblock->current_size);
  printf("bs = %ld\n", my_superblock->block_size);
  printf("fns = %lu\n", my_superblock->filename_size);
  printf("cfs = %lu\n", my_superblock->max_file_size);
  printf("mdfn = %u\n", my_superblock->max_dir_file_number);
}



void print_hole_table(hole_map* holes)
{
  printf("\n\nHOLE TABLE\n\n");
  printf("current_hole_number = %u\n", holes->current_hole_number);

  int i = 0;
  for (; i < MAX_HOLES; i++)
  {
    printf("start = %lu, end = %lu\n", holes->holes_table[i].start, holes->holes_table[i].end);

    if (holes->holes_table[i].end == 0)
    {
      return;
    }
  }
}



void print_MDS(MDS* mds)
{
  printf("\n\nMDS\n\n");
  printf("id = %u\n", mds->id);
  printf("size = %lu\n", mds->size);
  printf("type = %u\n", mds->type);
  printf("number_of_hard_links = %u\n", mds->number_of_hard_links);
  printf("parent_offset = %lu\n", mds->parent_offset);
  printf("creation_time = %lu\n", mds->creation_time);
  printf("access_time = %lu\n", mds->access_time);
  printf("modification_time = %lu\n", mds->modification_time);
  printf("blocks_using = %u\n", mds->blocks_using);
  printf("first_block = %lu\n", mds->first_block);
}



void print_Directory_Data_Block(Block* block, size_t fns)
{
  printf("\n\nBLOCK\n\n");

  size_t size_of_pair = fns + sizeof(size_t);
  uint pairs = block->bytes_used / size_of_pair;


  char* name = (char *) block->data;
  off_t* offset = pointer_to_offset(name, fns);

  int i = 0;
  for (; i < pairs; i++)
  {
    printf("Directory name: %s, offset: %lu\n", name, *offset);

    name = pointer_to_next_name(name, fns);
    offset = pointer_to_offset(name, fns);
  }
}
