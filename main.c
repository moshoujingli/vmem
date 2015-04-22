/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct disk *vdisk;


int* frame_page_index;

//fifo
int* frame_page_history_queue;
int head;
int tail;
int queue_lenth;

#define STATISTIC_SIZE 10
int page_fault_statistic[STATISTIC_SIZE];
int page_fault_statistic_index;


int get_read_count(){
    int i,count=0;
    for(i=0;i<STATISTIC_SIZE;i++){
        if(page_fault_statistic[i]==PROT_READ){
            count++;
        }
    }
    return count;
}

void enqueue_frame(int frame){
    frame_page_history_queue[(++tail)%queue_lenth] = frame;
    head = head==-1?0:head;
}
int dequeue_frame(){
    return head>-1?frame_page_history_queue[(head++)%queue_lenth]:-1;
}

int page_fault_count,disk_i,disk_o;

void disk_write_proxy( struct disk *d, int block, const char *data ){
    disk_write(d,block,data);
    disk_o++;
}
void disk_read_proxy( struct disk *d, int block, char *data ){
    disk_read(d,block,data);
    disk_i++;
}

//merge write
int get_frame_write_merge(struct page_table *pt){
    int frame,page,bit;

    int *read_only_frame = malloc(sizeof(int)*page_table_get_nframes(pt));
    // int *read_write_frame = malloc(sizeof(int)*page_table_get_nframes(pt));
    int read_only_frame_count = 0;
    // int read_write_frame_count = 0;
    for(frame=0;frame<page_table_get_nframes(pt);frame++){
        page = frame_page_index[frame];
        page_table_get_entry(pt,page,&frame,&bit);
        if((bit&PROT_WRITE) == 0){
            read_only_frame[read_only_frame_count++] = frame;
        }
    }
    if(read_only_frame_count>page_table_get_nframes(pt)/2)
        frame = read_only_frame[lrand48()%read_only_frame_count];
    else
        frame = lrand48()%page_table_get_nframes(pt);
    free(read_only_frame);
    return frame;
}
void flush_mem_write_merge(struct page_table *pt){
    int frame;
    for(frame=0;frame<page_table_get_nframes(pt);frame++){
        disk_write_proxy(vdisk,frame_page_index[frame],page_table_get_physmem(pt)+frame*PAGE_SIZE);
        page_table_set_entry(pt,frame_page_index[frame],frame,PROT_READ);
    }

}

void page_fault_handler_rand( struct page_table *pt, int page )
{
    page_fault_count++;
    int frame=0,
        bit=0;
    page_table_get_entry(pt,page,&frame,&bit);

    if(bit&PROT_READ){//a write action happend
        page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
    }else{//not loaded 
        for(frame=0;frame<page_table_get_nframes(pt);frame++){
            if(frame_page_index[frame]==-1){
                frame_page_index[frame]=page;
                disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
                page_table_set_entry(pt,page,frame,PROT_READ);
                return;
            }
        }
        //no frame not in use
        frame = lrand48()%page_table_get_nframes(pt);
        int outpage = frame_page_index[frame];
        page_table_get_entry(pt,outpage,&frame,&bit);
        if(bit&PROT_WRITE){
            disk_write_proxy(vdisk,outpage,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        }
        disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        page_table_set_entry(pt,outpage,frame,0);
        page_table_set_entry(pt,page,frame,PROT_READ);
        frame_page_index[frame] = page;
    }

}
void page_fault_handler_fifo( struct page_table *pt, int page )
{
    page_fault_count++;
    int frame=0,
        bit=0;
    page_table_get_entry(pt,page,&frame,&bit);

    if(bit&PROT_READ){//a write action happend
        page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
    }else{//not loaded 
        for(frame=0;frame<page_table_get_nframes(pt);frame++){
            if(frame_page_index[frame]==-1){
                frame_page_index[frame]=page;
                enqueue_frame(frame);
                disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
                page_table_set_entry(pt,page,frame,PROT_READ);
                return;
            }
        }
        //no frame not in use
        frame = dequeue_frame();
        int outpage = frame_page_index[frame];
        page_table_get_entry(pt,outpage,&frame,&bit);
        if(bit&PROT_WRITE){
            disk_write_proxy(vdisk,outpage,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        }
        disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        page_table_set_entry(pt,outpage,frame,0);
        page_table_set_entry(pt,page,frame,PROT_READ);
        frame_page_index[frame] = page;
        enqueue_frame(frame);
    }
}
void page_fault_handler_cust( struct page_table *pt, int page )
{

    page_fault_count++;
    int frame=0,
        bit=0;
    page_table_get_entry(pt,page,&frame,&bit);

    if(bit&PROT_READ){//a write action happend
        page_fault_statistic[page_fault_statistic_index++%STATISTIC_SIZE]=PROT_WRITE;
        page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
    }else{//not loaded 
        page_fault_statistic[page_fault_statistic_index++%STATISTIC_SIZE]=PROT_READ;
        for(frame=0;frame<page_table_get_nframes(pt);frame++){
            if(frame_page_index[frame]==-1){
                frame_page_index[frame]=page;
                disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
                page_table_set_entry(pt,page,frame,PROT_READ);
                return;
            }
        }
        //no frame not in use
        frame = get_frame_write_merge(pt);
        int outpage = frame_page_index[frame];
        page_table_get_entry(pt,outpage,&frame,&bit);
        if(bit&PROT_WRITE){
            disk_write_proxy(vdisk,outpage,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        }
        disk_read_proxy(vdisk,page,page_table_get_physmem(pt)+frame*PAGE_SIZE);
        page_table_set_entry(pt,outpage,frame,0);
        page_table_set_entry(pt,page,frame,PROT_READ);
        frame_page_index[frame] = page;
    }

}



int main( int argc, char *argv[] )
{
    if(argc!=5) {
        printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
        return 1;
    }

    int i;
    int npages = atoi(argv[1]);
    int nframes = atoi(argv[2]);
    const char *algorithm = argv[3]; 
    const char *program = argv[4];

    page_fault_handler_t page_fault_handler = NULL;

    if (!strcmp(algorithm,"rand")){
        page_fault_handler = page_fault_handler_rand;
    }else if(!strcmp(algorithm,"fifo")){
        page_fault_handler = page_fault_handler_fifo;
    }else if(!strcmp(algorithm,"custom")){
        page_fault_handler = page_fault_handler_cust;
    }else{
        fprintf(stderr,"unknown algorithm: %s\n",argv[3]);
    }

    frame_page_index = malloc(nframes*sizeof(int));
    for(i=0;i<nframes;i++) frame_page_index[i]=-1;
    frame_page_history_queue = malloc(nframes*sizeof(int));
    head=tail=-1;
    queue_lenth = nframes;

    vdisk = disk_open("myvirtualdisk",npages);
    if(!vdisk) {
        fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
        return 1;
    }


    struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
    if(!pt) {
        fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
        return 1;
    }

    char *virtmem = page_table_get_virtmem(pt);

    // char *physmem = page_table_get_physmem(pt);

    if(!strcmp(program,"sort")) {
        sort_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"scan")) {
        scan_program(virtmem,npages*PAGE_SIZE);

    } else if(!strcmp(program,"focus")) {
        focus_program(virtmem,npages*PAGE_SIZE);

    } else {
        fprintf(stderr,"unknown program: %s\n",argv[3]);

    }

    printf("%d %d %d\n",page_fault_count,disk_i,disk_o);

    free(frame_page_index);
    free(frame_page_history_queue);
    page_table_delete(pt);
    disk_close(vdisk);

    return 0;
}
