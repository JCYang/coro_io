//
// coro_io.hpp
// ~~~~~~~~
// A coroutines-ts wrapper for boost::asio
// A wrapper to use boost::asio as the backend in your coroutines,
// besides basic mapping of all(hopefully) the async operations, this wrapper library also provide a set of operations with timeout.
// 
// Copyright (c) 2017-2018 JC Yang (bandinfinite@gmail.com)
// All rights reserved.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//  No documentation yet.
//
#pragma once

#include "coro_io_impl.hpp"

namespace coro_io {
	
	// async_connect() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename EndPoint>
	std::enable_if_t<
		(std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> && std::is_same_v<EndPoint, impl::tcp_endpoint>) ||
		(std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket> && std::is_same_v<EndPoint, impl::udp_endpoint>),
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_connect(ObjType&& ts,
			EndPoint ep, boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([ep](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_connect(ep, [job_coordinator](const boost::system::error_code& ec) {
				job_coordinator->done_with_result(ec);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_connect without timeout
	template<typename ObjType, typename EndPoint>
	std::enable_if_t<
		(std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> && std::is_same_v<EndPoint, impl::tcp_endpoint>) ||
		(std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket> && std::is_same_v<EndPoint, impl::udp_endpoint>),
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<std::remove_reference_t<ObjType>>,
		impl::completion<std::remove_reference_t<ObjType>&&>>>
		async_connect(ObjType&& ts, EndPoint ep) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<NonRefObjType>,
			impl::completion<NonRefObjType&&>>;

		return completion_type([ep](completion_type& handler, NonRefObjType& ts) {
			ts.async_connect(ep, [&handler](const boost::system::error_code& ec) {
				handler.done_with_result(ec);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_send() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_send(ObjType&& ts, const ConstBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_send(buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_sent) {
				job_coordinator->done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_send() with flags + timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_send(ObjType&& ts, const ConstBufferSequence& buffer, boost::asio::socket_base::message_flags flags,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer, flags](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_send(buffer, flags, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_sent) {
				job_coordinator->done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_send() without timeout and flags
	template<typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_send(ObjType&& ts, const ConstBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer](completion_type& handler, NonRefObjType& ts) {
			ts.async_send(buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_sent) {
				handler.done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_send() with flags but no timeout
	template<typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_send(ObjType&& ts, const ConstBufferSequence& buffer, boost::asio::socket_base::message_flags flags) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer, flags](completion_type& handler, NonRefObjType& ts) {
			ts.async_send(buffer, flags, [&handler](const boost::system::error_code& ec, std::size_t bytes_sent) {
				handler.done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_receive() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_receive(ObjType&& ts, const MutableBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_receive(buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_received) {
				job_coordinator->done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_receive() with flags + timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_receive(ObjType&& ts, const MutableBufferSequence& buffer, boost::asio::socket_base::message_flags flags,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer, flags](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_receive(buffer, flags, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_received) {
				job_coordinator->done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_receive() without timeout and flags
	template<typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_receive(ObjType&& ts, const MutableBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer](completion_type& handler, NonRefObjType& ts) {
			ts.async_receive(buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_received) {
				handler.done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_receive() with flags but no timeout
	template<typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_receive(ObjType&& ts, const MutableBufferSequence& buffer, boost::asio::socket_base::message_flags flags) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer, flags](completion_type& handler, NonRefObjType& ts) {
			ts.async_receive(buffer, flags, [&handler](const boost::system::error_code& ec, std::size_t bytes_received) {
				handler.done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_write() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_write(ObjType&& ts, const ConstBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			async_write(ts, buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_sent) {
				job_coordinator->done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_write() without timeout
	template<typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_write(ObjType&& ts, const ConstBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer](completion_type& handler, NonRefObjType& ts) {
			async_write(ts, buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_sent) {
				handler.done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_read() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_read(ObjType&& ts, const MutableBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			async_read(ts, buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_received) {
				job_coordinator->done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_read() without timeout
	template<typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_read(ObjType&& ts, const MutableBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer](completion_type& handler, NonRefObjType& ts) {
			async_read(ts, buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_received) {
				handler.done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_write_at() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_write_at(ObjType&& ts, uint64_t offset, const ConstBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([offset, buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			async_write_at(ts, offset, buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_sent) {
				job_coordinator->done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_write_at() without timeout
	template<typename ObjType, typename ConstBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_write_at(ObjType&& ts, uint64_t offset, const ConstBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer, offset](completion_type& handler, NonRefObjType& ts) {
			async_write_at(ts, offset, buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_sent) {
				handler.done_with_result(ec, bytes_sent);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_read_at() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::rw_completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_read_at(ObjType&& ts, uint64_t offset, const MutableBufferSequence& buffer,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::rw_completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([offset, buffer](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			async_read_at(ts, offset, buffer, [job_coordinator](const boost::system::error_code& ec, std::size_t bytes_received) {
				job_coordinator->done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts), timeout);
	}

	// async_read_at() without timeout
	template<typename ObjType, typename MutableBufferSequence>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_socket> || std::is_same_v<std::remove_reference_t<ObjType>, impl::udp_socket>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<std::remove_reference_t<ObjType>>,
		impl::rw_completion<std::remove_reference_t<ObjType>&&>>>
		async_read_at(ObjType&& ts, uint64_t offset, const MutableBufferSequence& buffer) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::rw_completion<NonRefObjType>,
			impl::rw_completion<NonRefObjType&&>>;

		return completion_type([buffer, offset](completion_type& handler, NonRefObjType& ts) {
			async_read_at(ts, offset, buffer, [&handler](const boost::system::error_code& ec, std::size_t bytes_received) {
				handler.done_with_result(ec, bytes_received);
			});
		}, std::forward<ObjType>(ts));
	}

	template<typename ObjType>
	std::enable_if_t<
		(std::is_same_v<std::remove_reference_t<ObjType>, impl::deadline_timer> ||
			std::is_same_v<std::remove_reference_t<ObjType>, impl::high_resolution_timer> ||
			std::is_same_v<std::remove_reference_t<ObjType>, impl::system_timer> ||
			std::is_same_v<std::remove_reference_t<ObjType>, impl::steady_timer>),
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<std::remove_reference_t<ObjType>>,
		impl::completion<std::remove_reference_t<ObjType>&&>>>
		async_wait(ObjType&& ts) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<NonRefObjType>,
			impl::completion<NonRefObjType&&>>;

		return completion_type([](completion_type& handler, NonRefObjType& ts) {
			ts.async_wait([&handler](const boost::system::error_code& ec) {
				handler.done_with_result(ec);
			});
		}, std::forward<ObjType>(ts));
	}

	// async_accept() with timeout
	template<ResumeIn thread_to_resume, typename ObjType, typename Protocol, typename SocketService>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_acceptor>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>>,
		impl::completion_or_timeout<thread_to_resume, std::remove_reference_t<ObjType>&&>>>
		async_accept(ObjType&& ts, boost::asio::basic_socket<Protocol, SocketService>& socket,
			boost::posix_time::time_duration timeout) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion_or_timeout<thread_to_resume, NonRefObjType>,
			impl::completion_or_timeout<thread_to_resume, NonRefObjType&&>>;

		return completion_type([&socket](typename completion_type::coordinator_ptr& job_coordinator, NonRefObjType& ts) {
			ts.async_accept(socket, [job_coordinator](const boost::system::error_code& ec) {
				job_coordinator->done_with_result(ec);
			});
		}, std::forward<ObjType>(ts), timeout);
	}
	
	// async_accept() without timeout
	template<typename ObjType, typename Protocol, typename SocketService>
	std::enable_if_t<
		std::is_same_v<std::remove_reference_t<ObjType>, impl::tcp_acceptor>,
		std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<std::remove_reference_t<ObjType>>,
		impl::completion<std::remove_reference_t<ObjType>&&>>>
		async_accept(ObjType&& ts, boost::asio::basic_socket<Protocol, SocketService>& socket) {

		using NonRefObjType = std::remove_reference_t<ObjType>;

		using completion_type = std::conditional_t<std::is_lvalue_reference_v<ObjType>, impl::completion<NonRefObjType>,
			impl::completion<NonRefObjType&&>>;

		return completion_type([&socket](completion_type& handler, NonRefObjType& ts) {
			ts.async_accept(socket, [&handler](const boost::system::error_code& ec) {
				handler.done_with_result(ec);
			});
		}, std::forward<ObjType>(ts));
	}
}