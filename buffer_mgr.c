#include "buffer_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include "dberror.h"
#include "storage_mgr.h"

/*
 // Replacement Strategies
typedef enum ReplacementStrategy {
  RS_FIFO = 0,
  RS_LRU = 1,
  RS_CLOCK = 2,
  RS_LFU = 3,
  RS_LRU_K = 4
} ReplacementStrategy;

// Data Types and Structures
typedef int PageNumber;
#define NO_PAGE -1

typedef struct BM_BufferPool {
  char *pageFile;
  int numPages;
  ReplacementStrategy strategy;
  void *mgmtData; // use this one to store the bookkeeping info your buffer 
                  // manager needs for a buffer pool
  int numReadIO;                
  int numWriteIO;                
  int timer;
} BM_BufferPool;

typedef struct BM_PageHandle {
  PageNumber pageNum;
  char *data;
  bool dirty;
  int fixCounts;
  void* strategyAttribute;
} BM_PageHandle;

 */


// Buffer Manager Interface Pool Handling

/***************************************************************
 * Function Name: initBufferPool
 * 
 * Description: initBufferPool creates a new buffer pool with numPages page frames using the page replacement strategy strategy. The pool is used to cache pages from the page file with name pageFileName. Initially, all page frames should be empty. The page file should already exist, i.e., this method should not generate a new page file. stratData can be used to pass parameters for the page replacement strategy. For example, for LRU-k this could be the parameter k.
 *
 * Parameters: BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData.
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/24        Xiaoliang Wu                Not init pageHandle.
 *
***************************************************************/

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		  const int numPages, ReplacementStrategy strategy, 
		  void *stratData){
    FILE *file;

    file = fopen(pageFileName, "r");
    if(file == NULL){
        return RC_FILE_NOT_FOUND;
    }else{
        fclose(file);
    }
    bm->pageFile = (char *)pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    BM_PageHandle* buff = (BM_PageHandle *)calloc(numPages, sizeof(BM_PageHandle));
    bm->mgmtData = buff;
    bm->numReadIO = 0;
    bm->numWriteIO = 0;
    bm->timer = 0;
    return RC_OK;
}

/***************************************************************
 * Function Name: shutdownBufferPool
 * 
 * Description: shutdownBufferPool destroys a buffer pool. This method should free up all resources associated with buffer pool. For example, it should free the memory allocated for page frames. If the buffer pool contains any dirty pages, then these pages should be written back to disk before destroying the pool. It is an error to shutdown a buffer pool that has pinned pages.
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/24        Xiaoliang Wu                Complete.
 *      16/02/26        Xiaoliang Wu                Free buffer in pages.
 *		16/02/27		Xincheng Yang				Free fixCounts.
 *
***************************************************************/


RC shutdownBufferPool(BM_BufferPool *const bm){
    int *fixCounts;
    int i;
    RC RC_flag;

    fixCounts = getFixCounts(bm);
    for (i = 0; i < bm->numPages; ++i) {
        if(*(fixCounts+i)){
            return RC_SHUTDOWN_POOL_FAILED;
        }
    }

    RC_flag = forceFlushPool(bm);
    if(RC_flag != RC_OK){
        return RC_flag;
    }

    freePagesBuffer(bm);
	free(fixCounts);
    free(bm->mgmtData);
    free(bm->pageFile);
    return RC_OK;
}

/***************************************************************
 * Function Name: forceFlushPool
 * 
 * Description: forceFlushPool causes all dirty pages (with fix count 0) from the buffer pool to be written to disk.
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/25        Xiaoliang Wu                Complete, forcepage need set dirty to 0.
 *		16/02/27		Xincheng Yang				free fixCounts and dirtyFlags.
 *
***************************************************************/

RC forceFlushPool(BM_BufferPool *const bm){
    bool *dirtyFlags;
    int *fixCounts;
    int i;
    BM_PageHandle* page;
    RC RC_flag;

    dirtyFlags = getDirtyFlags(bm);
    fixCounts = getFixCounts(bm);

    for (i = 0; i < bm->numPages; ++i) {
        if(*(dirtyFlags+i)){
            if(*(fixCounts+i)){
                continue;
            }else{
                page = ((bm->mgmtData)+i);
                RC_flag = forcePage(bm, page);
                if(RC_flag != RC_OK){
                    return RC_flag;
                }
            }
        }
    }

	free(dirtyFlags);
	free(fixCounts);
    return RC_OK;
}

// Buffer Manager Interface Access Pages

/***************************************************************
 * Function Name: markDirty
 * 
 * Description:mark a page as dirty
 *
 * Parameters: BM_BufferPool *const bm, BM_PageHandle *const page
 *
 * Return:RC
 *
 * Author:Zhipeng Liu
 *
 * History:
 *      Date            Name                        Content
 *      02/25/16        Zhipeng Liu                 complete
***************************************************************/

RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	for(int i=0;i<bm->numPages;i++)
	{
		if((bm->mgmtData+i)->pageNum==page->pageNum)
		{
	        (bm->mgmtData+i)->dirty=1;
			break;
		}
	}
	return RC_OK;
}

/***************************************************************
 * Function Name: unpinPage
 * 
 * Description:unpin a page indicate by "page"
 *
 * Parameters:BM_BufferPool *const bm, BM_PageHandle *const page
 *
 * Return:RC
 *
 * Author:Zhipeng Liu
 *
 * History:
 *      Date            Name                        Content
 *      02/25/16        Zhipeng Liu                 complete
***************************************************************/

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	for(int i=0;i<bm->numPages;i++)
	{
		if((bm->mgmtData+i)->pageNum==page->pageNum)
		{
	        (bm->mgmtData+i)->fixCounts--;
			break;
		}
	}
	return RC_OK;
}

/***************************************************************
 * Function Name: 
 * 
 * Description:
 *
 * Parameters:
 *
 * Return:
 *
 * Author:
 *
 * History:
 *      Date            Name                        Content
 *
***************************************************************/

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	FILE *fp;
	fp=fopen(*(bm->pageFile),"rb+");
	fseek(fp,page->pageNum,SEEK_SET);
	fwrite(page->data,PAGE_SIZE,1,fp);
	numWriteIO++;
	page->dirty=0;
	return RC_OK;
}

/***************************************************************
 * Function Name: pinPage
 * 
 * Description:pin a page, if the page does not exist in memory, read it from file
 *
 * Parameters:BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum
 *
 * Return:RC
 *
 * Author:Zhipeng Liu
 *
 * History:
 *      Date            Name                        Content
 *02/25/16       Zhipeng Liu             imcomplete, need to implement the replace page part
***************************************************************/

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
	    const PageNumber pageNum)
{
	int pnum;
	int flag=0;
	for(int i=0;i<bm->numPages;i++)
	{
		if((bm->mgmtData+i)->pageNum==pageNum)
		{
			pnum=i;
			break;
		}
		if(i==bm->numPages-1)
			flag=1;
	}
	if(flag==1)
	{
	   pnum=替换函数;
	   updataAttribute(bm, page);
	   numReadIO++;
	}
	page->dirty=bm->mgmtData->dirty;
	page->data=bm->mgmtData->data;
	page->pageNum=pageNum;
	page->fixCounts++;
	return RC_OK;
}

// Statistics Interface

/***************************************************************
 * Function Name: getFrameContents 
 * 
 * Description: Returns an array of PageNumbers where the ith element is the number of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: PageNumber *
 *
 * Author: Xincheng Yang
 *
 * History:
 *      Date            Name                        Content
 *   2016/2/27		Xincheng Yang             first time to implement the function
 *
***************************************************************/
PageNumber *getFrameContents (BM_BufferPool *const bm){
    PageNumber *arr = (PageNumber*)malloc(bm->numPages * sizeof(PageNumber));
    BM_PageHandle *handle = bm->mgmtData;
    
    for(int i=0; i< bm->numPages; i++){
         arr[i] = (handle+i)->pageNum; 
    }
    return arr;
}

/***************************************************************
 * Function Name: getDirtyFlags 
 * 
 * Description: Returns an array of bools where the ith element is TRUE if the page stored in the ith page frame is dirty
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: bool *
 *
 * Author: Xincheng Yang
 *
 * History:
 *      Date            Name                        Content
 *   2016/2/27		Xincheng Yang             first time to implement the function
 *
***************************************************************/
bool *getDirtyFlags (BM_BufferPool *const bm){
    bool *arr = (bool*)malloc(bm->numPages * sizeof(bool));
    BM_PageHandle *handle = bm->mgmtData;
    
    for(int i=0; i< bm->numPages; i++){
         arr[i] = (handle+i)->dirty; 
    }
    return arr;
}

/***************************************************************
 * Function Name: getFixCounts 
 * 
 * Description: Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int *
 *
 * Author: Xincheng Yang
 *
 * History:
 *      Date            Name                        Content
 *   2016/2/27		Xincheng Yang             first time to implement the function
 *
***************************************************************/
int *getFixCounts (BM_BufferPool *const bm){
    int *arr = (int*)malloc(bm->numPages * sizeof(int));
    BM_PageHandle *handle = bm->mgmtData;
    
    for(int i=0; i< bm->numPages; i++){
         arr[i] = (handle+i)->fixCounts; 
    }
    return arr;
}

/***************************************************************
 * Function Name: getNumReadIO 
 * 
 * Description: Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int 
 *
 * Author: Xincheng Yang
 *
 * History:
 *      Date            Name                        Content
 *   2016/2/27		Xincheng Yang             first time to implement the function
 *
***************************************************************/
int getNumReadIO (BM_BufferPool *const bm){
	return bm->numReadIO;
}

/***************************************************************
 * Function Name: getNumWriteIO 
 * 
 * Description: Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int 
 *
 * Author: Xincheng Yang
 *
 * History:
 *      Date            Name                        Content
 *   2016/2/27		Xincheng Yang             first time to implement the function
 *
***************************************************************/
int getNumWriteIO (BM_BufferPool *const bm){
	return bm->numWriteIO;
}

/***************************************************************
 * Function Name: strategyFIFO
 * 
 * Description: decide use which frame to save data using FIFO.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int
 *
 * Author:
 *
 * History:
 *      Date            Name                        Content
 *
***************************************************************/

int strategyFIFO(BM_BufferPool *bm){
}

/***************************************************************
 * Function Name: strategyLRU
 * 
 * Description: decide use which frame to save data using LRU
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int
 *
 * Author:
 *
 * History:
 *      Date            Name                        Content
 *
***************************************************************/

int strategyLRU(BM_BufferPool *bm){
}

/***************************************************************
 * Function Name: strategyLRU_k
 * 
 * Description: decide use which frame to save data using LRU-k strategy.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int
 *
 * Author:
 *
 * History:
 *      Date            Name                        Content
 *
***************************************************************/

int strategyLRU_k(BM_BufferPool *bm){
}

/***************************************************************
 * Function Name: getAttributionArray
 * 
 * Description: return an array that includes all pages strategyAttribute.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int*
 *
 * Author:
 *
 * History:
 *      Date            Name                        Content
 *
***************************************************************/

int *getAttributionArray(BM_BufferPool *bm){
}

/***************************************************************
 * Function Name: freePagesBuffer
 * 
 * Description: free all pages in pool.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu 
 *
 * History:
 *      Date            Name                        Content
 *      16/02/26        Xiaoliang Wu                Complete.
 *
***************************************************************/

void freePagesBuffer(BM_BufferPool *bm){
    int i;
    for (i = 0; i < bm->numPages; ++i) {
        free((bm->mgmtData+i)->data);
        free((bm->mgmtData+i)->strategyAttribute);
    }
}

/***************************************************************
 * Function Name: updataAttribute
 * 
 * Description: modify the attribute about strategy. FIFO only use this function when page initial. LRU use this function when pinPage occurs.
 *
 * Parameters: BM_BufferPool *bm, BM_PageHandle *pageHandle
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu 
 *
 * History:
 *      Date            Name                        Content
 *      16/02/26        Xiaoliang Wu                FIFO, LRU complete.
 *
***************************************************************/

RC updataAttribute(BM_BufferPool *bm, BM_PageHandle *pageHandle){
    // initial page strategy attribute assign buffer
    if(pageHandle->strategyAttribute == NULL){

        if(bm->strategy == RS_FIFO || bm->strategy == RS_LRU){
            pageHandle->strategyAttribute = calloc(1, sizeof(int));
        }

    }

    // assign number
    if(bm->strategy == RS_FIFO || bm->strategy == RS_LRU){
        int * attribute;
        attribute = (int *)pageHandle->strategyAttribute;
        *attribute = (bm->timer); //???
        bm->timer++;
        return RC_OK;
    }

    return RC_STRATEGY_NOT_FOUND;
}
