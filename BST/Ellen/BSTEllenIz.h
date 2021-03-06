/*   
 *   File: bst_ellen.c
 *   Author: Tudor David <tudor.david@epfl.ch>
 *   Description: non-blocking binary search tree
 *      based on "Non-blocking Binary Search Trees"
 *      F. Ellen et al., PODC 2010
 *   bst_ellen.c is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* 
 * File:   ellen.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 3:56 PM
 */

#ifndef BST_ELLEN_IZ_H
#define BST_ELLEN_IZ_H

#include "include/record_manager.h"
#include "../pmem_utils.h"
#include "../../gc/ssmem.h"
namespace utils = pmem_utils;
/*
#define STATE_CLEAN 0
#define STATE_DFLAG 1
#define STATE_IFLAG 2
#define STATE_MARK 3


#define GETFLAG(ptr) (((uint64_t) (ptr)) & 3)
#define FLAG(ptr, flag) (info_t<skey_t, sval_t> *) ((((uint64_t) (ptr)) & 0xfffffffffffffffc) | (flag))
#define UNFLAG(ptr) (info_t<skey_t, sval_t> *) (((uint64_t) (ptr)) & 0xfffffffffffffffc)

template <typename skey_t, typename sval_t>
union info_t; 

template <typename skey_t, typename sval_t>
struct node_t;

template <typename skey_t, typename sval_t>
#if defined(CACHE_ALIGN)
struct alignas(64) iinfo_t {
#else
struct iinfo_t {
#endif
    node_t<skey_t, sval_t> * p;
    node_t<skey_t, sval_t> * new_internal;
    node_t<skey_t, sval_t> * l;
};

template <typename skey_t, typename sval_t>
#if defined(CACHE_ALIGN)
struct alignas(64) dinfo_t {
#else
struct dinfo_t {
#endif
    node_t<skey_t, sval_t> * gp;
    node_t<skey_t, sval_t> * p;
    node_t<skey_t, sval_t> * l;
    info_t<skey_t, sval_t> * pupdate;
};

template <typename skey_t, typename sval_t>
#if defined(CACHE_ALIGN)
union alignas(64) info_t {
#else
union info_t {
#endif
    iinfo_t<skey_t, sval_t> iinfo;
    dinfo_t<skey_t, sval_t> dinfo;
};

template <typename skey_t, typename sval_t>
#if defined(CACHE_ALIGN)
struct alignas(64) node_t {
#else
struct node_t {
#endif
    skey_t key;
    sval_t value;
    info_t<skey_t, sval_t> * volatile update;
    node_t<skey_t, sval_t> * volatile left;
    node_t<skey_t, sval_t> * volatile right;
    #ifdef OURS_OP
        node_t<skey_t, sval_t> * volatile original_parent;
    #endif
    #ifdef MARK_FLUSHED
        volatile int flushed;
    #endif
};
*/
template <typename skey_t, typename sval_t, class RecMgr>
class BSTEllenIz {
private:
PAD;
    const unsigned int idx_id;
PAD;
    node_t<skey_t, sval_t> * root;
PAD;
    const int NUM_THREADS;
    const skey_t BST_KEY_MIN;
    const skey_t BST_KEY_MAX;
    const sval_t NO_VALUE;
PAD;
    RecMgr * const recmgr;
PAD;
    int init[MAX_THREADS_POW2] = {0,}; // this suffers from false sharing, but is only touched once per thread! so no worries.
PAD;

    bool bst_cas_child(const int tid, node_t<skey_t, sval_t> * parent, node_t<skey_t, sval_t> * old, node_t<skey_t, sval_t> * nnode);
    void bst_help(const int tid, info_t<skey_t, sval_t>* u/*, EpochThread epoch*/);
    void bst_help_marked(const int tid, info_t<skey_t, sval_t>* op/*, EpochThread epoch*/);
    bool bst_help_delete(const int tid, info_t<skey_t, sval_t>* op/*, EpochThread epoch*/);
    void bst_help_insert(const int tid, info_t<skey_t, sval_t> * op/*, EpochThread epoch*/);

    node_t<skey_t, sval_t> * create_node(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> * left, node_t<skey_t, sval_t> * right/*, EpochThread epoch*/) {
        //auto result = recmgr->template allocate<node_t<skey_t, sval_t>>(tid);
	auto result = static_cast<node_t<skey_t, sval_t>*>(ssmem_alloc(alloc, sizeof(node_t<skey_t, sval_t>)));
	//auto result = (node_t<skey_t, sval_t>*)EpochAllocNode(epoch, sizeof(node_t<skey_t, sval_t>));
        if (result == NULL) setbench_error("out of memory");
        result->key = key;
        FLUSH(tid, &result->key);
        result->value = value;
        FLUSH(tid, &result->value);
        result->update = NULL;
        FLUSH(tid, &result->update);
        result->left = left;
        FLUSH(tid, &result->left);
        result->right = right;
        FLUSH(tid, &result->right);
        return result;
    }

    node_t<skey_t, sval_t> * create_node_initial(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> * left, node_t<skey_t, sval_t> * right/*, EpochThread epoch*/) {
        auto result = recmgr->template allocate<node_t<skey_t, sval_t>>(tid);
        //auto result = static_cast<node_t<skey_t, sval_t>*>(ssmem_alloc(alloc, sizeof(node_t<skey_t, sval_t>)));
        //auto result = (node_t<skey_t, sval_t>*)EpochAllocNode(epoch, sizeof(node_t<skey_t, sval_t>));
        if (result == NULL) setbench_error("out of memory");
        result->key = key;
        FLUSH(tid, &result->key);
        result->value = value;
        FLUSH(tid, &result->value);
        result->update = NULL;
        FLUSH(tid, &result->update);
        result->left = left;
        FLUSH(tid, &result->left);
        result->right = right;
        FLUSH(tid, &result->right);
        return result;
    }
    
    info_t<skey_t, sval_t> * create_iinfo_t(const int tid, node_t<skey_t, sval_t> * p, node_t<skey_t, sval_t> * ni, node_t<skey_t, sval_t> * l/*, EpochThread epoch*/) {
        //auto result = recmgr->template allocate<info_t<skey_t, sval_t>>(tid);
        //auto result = (info_t<skey_t, sval_t>*)EpochAllocNode(epoch, sizeof(info_t<skey_t, sval_t>));
	auto result = static_cast<info_t<skey_t, sval_t>*>(ssmem_alloc(allocI, sizeof(info_t<skey_t, sval_t>)));
	if (result == NULL) setbench_error("out of memory");
        result->iinfo.p = p;
        FLUSH(tid, &result->iinfo.p);
        result->iinfo.new_internal = ni;
        FLUSH(tid, &result->iinfo.new_internal);
        result->iinfo.l = l;
        FLUSH(tid, &result->iinfo.l);
        return result;
    }

    info_t<skey_t, sval_t> * create_dinfo_t(const int tid, node_t<skey_t, sval_t> * gp, node_t<skey_t, sval_t> * p, node_t<skey_t, sval_t> * l, info_t<skey_t, sval_t> * u/*, EpochThread epoch*/) {
        //auto result = recmgr->template allocate<info_t<skey_t, sval_t>>(tid);
        //auto result = (info_t<skey_t, sval_t>*)EpochAllocNode(epoch, sizeof(info_t<skey_t, sval_t>));
	auto result = static_cast<info_t<skey_t, sval_t>*>(ssmem_alloc(allocI, sizeof(info_t<skey_t, sval_t>)));
	if (result == NULL) setbench_error("out of memory");
        result->dinfo.gp = gp;
        FLUSH(tid, &result->dinfo.gp);
        result->dinfo.p = p;
        FLUSH(tid, &result->dinfo.p);
        result->dinfo.l = l;
        FLUSH(tid, &result->dinfo.l);
        result->dinfo.pupdate = u;
        FLUSH(tid, &result->dinfo.pupdate);
        return result;
    }
public:

    BSTEllenIz(const int _NUM_THREADS, const skey_t& _BST_KEY_MIN, const skey_t& _BST_KEY_MAX, const sval_t& _VALUE_RESERVED, unsigned int id/*, EpochThread epoch*/)
    : idx_id(id), NUM_THREADS(_NUM_THREADS), BST_KEY_MIN(_BST_KEY_MIN), BST_KEY_MAX(_BST_KEY_MAX), NO_VALUE(_VALUE_RESERVED), recmgr(new RecMgr(NUM_THREADS)) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid); // enter an initial quiescent state.

        auto i1 = create_node_initial(tid, BST_KEY_MAX, NO_VALUE, NULL, NULL/*, epoch*/);
        auto i2 = create_node_initial(tid, BST_KEY_MAX, NO_VALUE, NULL, NULL/*, epoch*/);
        root = create_node_initial(tid, BST_KEY_MAX, NO_VALUE, i1, i2/*, epoch*/);
    }

    ~BSTEllenIz() {
        recmgr->printStatus();
        delete recmgr;
    }
    
    void printTree(node_t<skey_t, sval_t> * node, int depth) {
        //if (depth > 5) return;
        std::cout<<"depth="<<depth<<" key="<<node->key<<std::endl;
        if (node->left) printTree(node->left, depth+1);
        if (node->right) printTree(node->right, depth+1);
    }
    void printTree() {
        printTree(root, 0);
    }

    void initThread(const int tid) {
        if (init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->initThread(tid);
    }

    void deinitThread(const int tid) {
        if (!init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }

    sval_t bst_find(const int tid, skey_t key/*, EpochThread epoch*/);
    sval_t bst_insert(const int tid, skey_t key, sval_t value/*, EpochThread epoch*/);
    sval_t bst_delete(const int tid, skey_t key/*, EpochThread epoch*/);
    
    node_t<skey_t, sval_t> * get_root(){
        return root; 
    }
    
    RecMgr * debugGetRecMgr() {
        return recmgr;
    }    
};


#endif /* ELLEN_H */


