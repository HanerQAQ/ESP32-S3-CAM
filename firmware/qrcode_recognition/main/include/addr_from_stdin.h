/* Common utilities for entering socket addresses / 套接字地址输入接口的通用工具
    The API get_addr_from_stdin() is mainly used by socket client examples which read IP address from stdin (if configured).
    This option is typically used in the CI, but could be enabled in the project configuration.
    In that case this component is used to receive a string that is evaluated and processed to output
    socket structures to open a connectio
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read and parse an IP address from standard input / 从标准输入读取并解析 IP 地址
 *
 * This API parses the address with getaddrinfo() / 此接口使用 getaddrinfo() 解析输入地址
 * and fills sockaddr_storage for opening an IPv4 or IPv6 socket / 并填充 sockaddr_storage，以便打开 IPv4 或 IPv6 套接字
 * The IP protocol is inferred from the address string. / IP 协议类型根据地址字符串推断。
 *
 * @param[in] port Expected connection port / 预期连接使用的端口号
 * @param[in] sock_type Expected socket type: SOCK_STREAM or SOCK_DGRAM / 预期的套接字类型：SOCK_STREAM 或 SOCK_DGRAM
 * @param[out] ip_protocol Detected IP protocol: IPPROTO_IP or IPPROTO_IP6 / 检测到的 IP 协议：IPPROTO_IP 或 IPPROTO_IP6
 * @param[out] addr_family Detected address family: AF_INET or AF_INET6 / 检测到的地址族：AF_INET 或 AF_INET6
 * @param[out] dest_addr sockaddr_storage for IPv4 or IPv6 / 用于 IPv4 或 IPv6 的 sockaddr_storage 结构体
 * @return ESP_OK on success; ESP_FAIL otherwise / 成功时返回 ESP_OK，否则返回 ESP_FAIL
 */
esp_err_t get_addr_from_stdin(int port, int sock_type,
                              int *ip_protocol,
                              int *addr_family,
                              struct sockaddr_storage *dest_addr);

#ifdef __cplusplus
}
#endif
