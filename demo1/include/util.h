/**
 * @file util.h
 * @brief 矩阵/向量工具函数
 */

#pragma once

/**
 * @brief 分配一维双精度向量
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 * @return 向量指针（从 nl 开始索引）
 */
double* dvector(long nl, long nh);

/**
 * @brief 释放一维双精度向量
 * @param[in] m  向量指针
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 */
void free_dvector(double* m, long nl, long nh);

/**
 * @brief 分配一维整型向量
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 * @return 向量指针
 */
int* ivector(long nl, long nh);

/**
 * @brief 释放一维整型向量
 * @param[in] m  向量指针
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 */
void free_ivector(int* m, long nl, long nh);

/**
 * @brief 分配三维指针数组
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 * @return 三维指针
 */
double*** dvector_3d(long nl, long nh);

/**
 * @brief 释放三维指针数组
 * @param[in] m  三维指针
 * @param[in] nl 起始索引
 * @param[in] nh 结束索引
 */
void free_dvector_3d(double*** m, long nl, long nh);

/**
 * @brief 分配二维双精度矩阵
 * @param[in] nrl 行起始索引
 * @param[in] nrh 行结束索引
 * @param[in] ncl 列起始索引
 * @param[in] nch 列结束索引
 * @return 矩阵指针
 */
double** dmatrix(long nrl, long nrh, long ncl, long nch);

/**
 * @brief 释放二维双精度矩阵
 * @param[in] m   矩阵指针
 * @param[in] nrl 行起始索引
 * @param[in] nrh 行结束索引
 * @param[in] ncl 列起始索引
 * @param[in] nch 列结束索引
 */
void free_dmatrix(double** m, long nrl, long nrh, long ncl, long nch);

/**
 * @brief 矩阵加法 a = b + c
 */
void mat_add(double** a, double** b, double** c, int xl, int xr, int yl, int yr);

/**
 * @brief 向量置零
 */
void zero_vector(double* a, int xl, int xr);

/**
 * @brief 矩阵置零
 */
void zero_matrix(double** a, int xl, int xr, int yl, int yr);

/**
 * @brief 矩阵复制 a = b
 */
void mat_copy(double** a, double** b, int xl, int xr, int yl, int yr);

/**
 * @brief 向量复制 a = b
 */
void vec_copy(double* a, double* b, int xl, int xr);

/**
 * @brief 矩阵减法 a = b - c
 */
void mat_sub(double** a, double** b, double** c, int nrl, int nrh, int ncl, int nch);

/**
 * @brief 向量最大值
 */
double vec_max(double* a, int nrl, int nrh);

/**
 * @brief 向量最小值
 */
double vec_min(double* a, int nrl, int nrh);

/**
 * @brief 矩阵绝对值最大值
 */
double mat_max(double** a, int nrl, int nrh, int ncl, int nch);