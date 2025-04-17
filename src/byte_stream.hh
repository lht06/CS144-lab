#pragma once // 表示这个头文件只会被包含一次，防止重复包含（等同于传统的 include guard）。

#include <cstdint>     //cstdint: 提供固定宽度的整数类型（如 uint64_t）。
#include <string>      //string: 用于数据存储的缓冲区。
#include <string_view> //string_view: 提供轻量的字符串读取视图，不拷贝数据。

class Reader;
class Writer;

class ByteStream // 这个类是流的核心，用来维护缓冲区（一个字符串）及其读写状态。
{
public:
  explicit ByteStream( uint64_t capacity ); // 构造函数：显式构造 ByteStream，传入缓冲区最大容量。 explicit
                                            // 的作用是：防止构造函数发生“隐式类型转换”（implicit conversion）。

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  // 返回类型：Reader&  返回一个 Reader 的引用；
  // 函数名：reader()；
  Reader& reader();
  // 前面的 const Reader&：表示返回的是一个“只读的 Reader 引用”；
  // 后面的 const：表示这个函数是个“只读函数”，不能修改 ByteStream 的内容；
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;

  void set_error() { error_ = true; };       // Signal that the stream suffered an error.
  bool has_error() const { return error_; }; // Has the stream had an error?

protected:
  // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.
  std::string buffer_ {};   // 缓冲区
  bool writeClosed_ {};     // 写是否关闭
  uint64_t haveWritten_ {}; // 写了多少
  uint64_t haveRead_ {};    // 读了多少
  uint64_t capacity_ {};    // 最大容量
  uint64_t offset_ {};      // 偏移量 用来优化pop
  bool error_ {};           // 是否错误
};

class Writer : public ByteStream
{
public:
  void push( std::string data ); // Push data to stream, but only as much as available capacity allows.
  void close();                  // Signal that the stream has reached its ending. Nothing more will be written.

  bool is_closed() const;              // Has the stream been closed?
  uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:                          // peek的意思：喵一眼
  std::string_view peek() const; // Peek at the next bytes in the buffer 查看当前缓冲区中可读的内容（不移除数据）。
  void pop( uint64_t len ); // Remove `len` bytes from the buffer

  bool is_finished() const;        // Is the stream finished (closed and fully popped)?
  uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `max_len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t max_len, std::string& out );