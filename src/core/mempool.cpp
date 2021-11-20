#include "mempool.hpp"
#include <iostream>
using namespace std;

#define MAX_FUTURE_BLOCKS 5

void mempool_sync(MemPool& mempool) {

}

MemPool::MemPool(HostManager& h, BlockChain &b) : hosts(h), blockchain(b) {
}


void MemPool::sync() {
    this->syncThread.push_back(std::thread(mempool_sync, ref(*this)));
}

ExecutionStatus MemPool::addTransaction(Transaction t) {
    ExecutionStatus status;
    if (t.getBlockId() <= this->blockchain.getBlockCount()) {
        return EXPIRED_TRANSACTION;
    } else if (t.getBlockId() == this->blockchain.getBlockCount() + 1) { // case 1: add to latest block queue + verify
        status = this->blockchain.verifyTransaction(t);
        if (status != SUCCESS) {
            return status;
        } else {
            this->lock.lock();
            if (this->transactionQueue[t.getBlockId()].size() < MAX_TRANSACTIONS_PER_BLOCK) {
                this->transactionQueue[t.getBlockId()].insert(t);
                status = SUCCESS;
            } else {
                status = QUEUE_FULL;
            }
            this->lock.unlock();
            return status;
        }
    } else if (t.getBlockId() > this->blockchain.getBlockCount() + 1){ // case 2: add to a future blocks queue, no verify
        if (t.getBlockId() < this->blockchain.getBlockCount() + MAX_FUTURE_BLOCKS) {   // limit to queueing up to N blocks in future
            this->lock.lock();
            if (this->transactionQueue[t.getBlockId()].size() < MAX_TRANSACTIONS_PER_BLOCK) {
                status = SUCCESS;
                this->transactionQueue[t.getBlockId()].insert(t);
            } else {
                status = QUEUE_FULL;
            }
            this->lock.unlock();
            return status;
        } else {
            return BLOCK_ID_TOO_LARGE;
        }
    }
    return UNKNOWN_ERROR;
}

void MemPool::finishBlock(int blockId) {
    this->lock.lock();
    // erase all transactions prior to this block
    vector<int> toDelete;
    for (auto pair : this->transactionQueue) {
        if (pair.first <= blockId) {
            toDelete.push_back(pair.first);
        }
    }
    for (auto blockId : toDelete) {
        this->transactionQueue.erase(blockId);
    }
    // validate transactions scheduled for the next block if needed:
    if (this->transactionQueue.find(blockId+1) != this->transactionQueue.end()) {
        set<Transaction>& currTransactions =  this->transactionQueue[blockId+1];
        for (auto it = currTransactions.begin(); it != currTransactions.end(); ) {
            ExecutionStatus status = this->blockchain.verifyTransaction(*it);
            if (status != SUCCESS) {
                currTransactions.erase(it++);
            } else {
                ++it;
            }
        }
    }
    this->lock.unlock();
}

set<Transaction>& MemPool::getTransactions(int blockId) {
    return this->transactionQueue[blockId];
}