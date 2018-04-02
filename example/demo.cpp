#include "coro_io.hpp"

#include <vector>
#include <thread>
#include <memory>

boost::asio::io_service g_io_service;
boost::asio::io_service::work g_io_work(g_io_service);

// Hey! this is a coroutine, any functions with co_await keyword in its body become coroutines
void demo_coro_A() {
	auto tcp_socket = boost::asio::ip::tcp::socket(g_io_service);
	auto remote_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string("10.0.28.88"), 16888);
	// timeout and connect success completion are two distinct operation that need to be sync
	// because this coroutine is first called in main thread, and resume in io_thread
	// we need to explicitly tell it ResumeIn::other_thread to indicate that sync primitive is required 
	auto connect_err = co_await coro_io::async_connect<coro_io::ResumeIn::other_thread>(tcp_socket, remote_endpoint, boost::posix_time::seconds(3));

	// when resumed, the execution continue in io_thread now!
	if (!connect_err) {
		// how about you'd like to wait for a moment before taking next act?
		boost::asio::deadline_timer demo_timer( g_io_service );
		demo_timer.expires_from_now(boost::posix_time::seconds(10));
		auto wait_err = co_await coro_io::async_wait(demo_timer);
		// since execution is now in io_thread, we do not need sync primitive, we can optimize it now		
		auto[write_err, bytes_written] = co_await coro_io::async_write<coro_io::ResumeIn::this_thread>(tcp_socket, boost::asio::buffer("Any non-fancy stuff"), boost::posix_time::seconds(3));
		auto read_buf = std::vector<unsigned char>(100);
		auto[read_err, bytes_read] = co_await coro_io::async_read<coro_io::ResumeIn::this_thread>(tcp_socket, boost::asio::buffer(read_buf), boost::posix_time::seconds(3));

		auto ta = boost::asio::ip::tcp::acceptor(g_io_service);
		// operation without timeout are safe to use wherever you like and need no sync.
		// this is one of the main sellpoint of coroutines
		auto accept_err = co_await coro_io::async_accept(ta, tcp_socket);
		// ... 
		// whatever your app like to do consequently
	}
}

// And this one too
void demo_coro_B() {
	auto tcp_socket = boost::asio::ip::tcp::socket(g_io_service);
	auto remote_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::from_string("10.0.28.99"), 16888);
	// operation without timeout are safe to use wherever you like and need no sync.
	// this is one of the main sellpoint of coroutines
	auto connect_err = co_await coro_io::async_connect(tcp_socket, remote_endpoint);

	if (!connect_err) {
		// because this coro is called in the io_thread, the same thread where to resume in when the io complete or timeout.
		// so the completion and timeout does not need sync primitive to work correctly, just a pay-what-you-need-to-pay path to optimize this use case
		// i.e, you should use this when multiplexing a single io_thread for all io operations.
		auto[write_err, bytes_written] = co_await coro_io::async_write<coro_io::ResumeIn::this_thread>(tcp_socket, boost::asio::buffer("Any non-fancy stuff"), boost::posix_time::seconds(3));
		auto read_buf = std::vector<unsigned char>(100);
		auto[read_err, bytes_read] = co_await coro_io::async_read<coro_io::ResumeIn::this_thread>(tcp_socket, boost::asio::buffer(read_buf), boost::posix_time::seconds(3));

		auto ta = boost::asio::ip::tcp::acceptor(g_io_service);
		auto accept_err = co_await coro_io::async_accept(ta, tcp_socket);
		// ... 
		// whatever your app like to do consequently
	}
}

int main() {
	std::thread io_thread{ []() {g_io_service.run(); } };
	// coro called in main thread
	demo_coro_A();

	// coro called in io_thread
	g_io_service.post([]() { demo_coro_B(); });
	std::this_thread::sleep_for(std::chrono::hours(1));
	return 0;
}