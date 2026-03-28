# Multithreaded Web Crawler in C++

A multithreaded web crawler written in C++ using Windows sockets. The program reads a list of seed URLs from an input file, distributes them across worker threads, performs DNS lookup and HTTP requests, checks `/robots.txt`, downloads pages, parses links, and reports crawl statistics.

## Overview

This project was built to demonstrate:

- multithreaded systems programming in C++
- socket-based HTTP communication
- synchronization of shared resources
- URL parsing and HTML link extraction
- performance tracking during concurrent crawling

The crawler uses multiple worker threads to process a shared queue of input URLs. For each URL, it validates the host, performs DNS resolution, avoids duplicate hosts and duplicate IPs, checks `/robots.txt`, fetches the page, parses links from successful `2xx` responses, and prints crawl statistics throughout execution.

## Features

- Multithreaded worker-based crawling
- Shared thread-safe URL queue
- DNS lookup for each host
- Duplicate host filtering
- Duplicate IP filtering
- `/robots.txt` check before fetching a page
- HTTP response code tracking (`2xx`, `3xx`, `4xx`, `5xx`, other)
- HTML link extraction using `HTMLParserBase`
- Periodic performance and throughput statistics
- Detection of `tamu.edu` links found on crawled `2xx` pages

## Tech Stack

- C++
- Visual Studio / MSVC
- WinSock2
- Custom URL parsing
- Provided HTML parsing library (`HTMLParserBase`)

## Project Structure

```text
.
├── Crawler.cpp
├── Crawler.h
├── main.cpp
├── Parse.cpp
├── Parse.h
├── socket.cpp
├── Socket.h
├── HTMLParserBase.h
├── pch.cpp
├── pch.h
├── README.md
└── .gitignore
