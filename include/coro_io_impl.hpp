//
// coro_io_impl.hpp
// ~~~~~~~~
// Implementation details of coro_io
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
#ifndef CORO_IO_IMPL_HPP_
#define CORO_IO_IMPL_HPP_

#include <memory>

// if you're using clang + libstdc++(typical combination in linux), we need coroutine.h to bootstrap it
#ifdef __GLIBCXX__
#include "coroutine.h"
#else
// for msvc and clang+libc++, TS is correctly supported
#include <experimental/coroutine>
#endif

#include <type_traits>
#include <optional>
#include <mutex>
#include "boost/asio.hpp"
#include "boost/asio/high_resolution_timer.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/system_timer.hpp"


namespace std {
	namespace experimental {
		template<typename... Args>
		struct coroutine_traits<void, Args...> {
			struct promise_type {
				void get_return_object() {}
				static void get_return_object_on_allocation_failure() {}
				auto initial_suspend() { return suspend_never{}; }
				auto final_suspend() { return suspend_never{}; }
				void unhandled_exception() {}
				void return_void() {}
			};
		};
	}
}

namespace coro_io {
	enum class ResumeIn {
		this_thread,
		other_thread
	};

	namespace impl {
		template <ResumeIn thread_to_resume, typename ObjType, typename... Rs>
		class JobCoordinator;

		template <typename ObjType, typename... Rs>
		class JobCoordinator<ResumeIn::other_thread, ObjType, Rs...> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType& s, const duration& timeout) :
				m_asio_obj_ref(s), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_result = std::forward_as_tuple(ec, results...);
						if (m_timeout_timer.has_value()) {
							m_timeout_timer.value().cancel();
							m_timeout = boost::posix_time::seconds(0);
						}
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj_ref);

				std::lock_guard<std::mutex> state_lock(m_state_mutex);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj_ref.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_result; }
		private:
			void cancel_task() {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_asio_obj_ref.cancel();
						std::get<0>(m_result) = boost::system::errc::make_error_code(boost::system::errc::timed_out);
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			std::mutex				m_state_mutex;
			bool				m_coro_resumed = false;
			ObjType&			m_asio_obj_ref;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_result;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template <typename ObjType, typename... Rs>
		class JobCoordinator<ResumeIn::other_thread, ObjType&&, Rs...> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType&& s, const duration& timeout) :
				m_asio_obj(std::move(s)), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_result = std::forward_as_tuple(ec, results...);
						if (m_timeout_timer.has_value()) {
							m_timeout_timer.value().cancel();
							m_timeout = boost::posix_time::seconds(0);
						}
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj);
				std::lock_guard<std::mutex> state_lock(m_state_mutex);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_result; }
		private:
			void cancel_task() {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_asio_obj.cancel();
						std::get<0>(m_result) = boost::system::errc::make_error_code(boost::system::errc::timed_out);
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			std::mutex			m_state_mutex;
			bool			m_coro_resumed = false;
			ObjType			m_asio_obj;
			async_task		m_async_work;
			coro_handle		m_coro;
			result_type		m_result;
			duration		m_timeout;
			asio_timer		m_timeout_timer;
		};


		template <typename ObjType>
		class JobCoordinator<ResumeIn::other_thread, ObjType> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = boost::system::error_code;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType& s, const duration& timeout) :
				m_asio_obj_ref(s), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			void done_with_result(boost::system::error_code ec) {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_error_code = ec;
						if (m_timeout_timer.has_value()) {
							m_timeout_timer.value().cancel();
							m_timeout = boost::posix_time::seconds(0);
						}
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj_ref);
				std::lock_guard<std::mutex> state_lock(m_state_mutex);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj_ref.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_error_code; }
		private:
			void cancel_task() {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_asio_obj_ref.cancel();
						m_error_code = boost::system::errc::make_error_code(boost::system::errc::timed_out);
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			std::mutex				m_state_mutex;
			bool				m_coro_resumed = false;
			ObjType&			m_asio_obj_ref;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_error_code;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template <typename ObjType>
		class JobCoordinator<ResumeIn::other_thread, ObjType&&> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = boost::system::error_code;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType&& s, const duration& timeout) :
				m_asio_obj(std::move(s)), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			void done_with_result(boost::system::error_code ec) {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_error_code = ec;
						if (m_timeout_timer.has_value()) {
							m_timeout_timer.value().cancel();
							m_timeout = boost::posix_time::seconds(0);
						}
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj);
				std::lock_guard<std::mutex> state_lock(m_state_mutex);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_error_code; }
		private:
			void cancel_task() {
				auto done = false;
				{
					std::lock_guard<std::mutex> state_lock(m_state_mutex);
					if (!m_coro_resumed && m_coro) {
						m_asio_obj.cancel();
						m_error_code = boost::system::errc::make_error_code(boost::system::errc::timed_out);
						m_coro_resumed = true;
						done = true;
					}
				}
				if (done)
					m_coro.resume();
			}

			std::mutex				m_state_mutex;
			bool				m_coro_resumed = false;
			ObjType				m_asio_obj;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_error_code;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template <typename ObjType, typename... Rs>
		class JobCoordinator<ResumeIn::this_thread, ObjType, Rs...> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType& s, const duration& timeout) :
				m_asio_obj_ref(s), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				if (!m_coro_resumed && m_coro) {
					m_result = std::forward_as_tuple(ec, results...);
					if (m_timeout_timer.has_value())
						m_timeout_timer.value().cancel();
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj_ref);

				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj_ref.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_result; }
		private:
			void cancel_task() {
				if (!m_coro_resumed && m_coro) {
					m_asio_obj_ref.cancel();
					std::get<0>(m_result) = boost::system::errc::make_error_code(boost::system::errc::timed_out);
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			bool				m_coro_resumed = false;
			ObjType&			m_asio_obj_ref;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_result;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template <typename ObjType, typename... Rs>
		class JobCoordinator<ResumeIn::this_thread, ObjType&&, Rs...> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType&& s, const duration& timeout) :
				m_asio_obj(std::move(s)), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				if (!m_coro_resumed && m_coro) {
					m_result = std::forward_as_tuple(ec, results...);
					if (m_timeout_timer.has_value())
						m_timeout_timer.value().cancel();
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_result; }
		private:
			void cancel_task() {
				if (!m_coro_resumed && m_coro) {
					m_asio_obj.cancel();
					std::get<0>(m_result) = boost::system::errc::make_error_code(boost::system::errc::timed_out);
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			bool				m_coro_resumed = false;
			ObjType				m_asio_obj;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_result;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};


		template <typename ObjType>
		class JobCoordinator<ResumeIn::this_thread, ObjType> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = boost::system::error_code;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType& s, const duration& timeout) :
				m_asio_obj_ref(s), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			void done_with_result(boost::system::error_code ec) {
				if (!m_coro_resumed && m_coro) {
					m_error_code = ec;
					if (m_timeout_timer.has_value())
						m_timeout_timer.value().cancel();
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj_ref);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj_ref.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_error_code; }
		private:
			void cancel_task() {
				if (!m_coro_resumed && m_coro) {
					m_asio_obj_ref.cancel();
					m_error_code = boost::system::errc::make_error_code(boost::system::errc::timed_out);
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			bool				m_coro_resumed = false;
			ObjType&			m_asio_obj_ref;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_error_code;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template <typename ObjType>
		class JobCoordinator<ResumeIn::this_thread, ObjType&&> {
		public:
			using self_shared_ptr = std::shared_ptr<JobCoordinator>;
			using asio_timer = std::optional<boost::asio::deadline_timer>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using duration = boost::posix_time::time_duration;
			using result_type = boost::system::error_code;
			using async_task = std::function<void(self_shared_ptr&, ObjType&)>;

			JobCoordinator(async_task&& async_work, ObjType&& s, const duration& timeout) :
				m_asio_obj(std::move(s)), m_async_work(std::move(async_work)), m_timeout(timeout) {}

			void done_with_result(boost::system::error_code ec) {
				if (!m_coro_resumed && m_coro) {
					m_error_code = ec;
					if (m_timeout_timer.has_value())
						m_timeout_timer.value().cancel();
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			void start_work(coro_handle coro, self_shared_ptr this_coordinator) {
				m_coro = coro;
				m_async_work(this_coordinator, m_asio_obj);
				if (m_timeout.total_milliseconds() > 0) {
					m_timeout_timer.emplace(m_asio_obj.get_io_service(), m_timeout);
					m_timeout_timer.value().async_wait([this_coordinator](const boost::system::error_code& ec) {
						if (ec != boost::asio::error::operation_aborted)
							this_coordinator->cancel_task();
					});
				}
			}

			result_type get_result() { return m_error_code; }
		private:
			void cancel_task() {
				if (!m_coro_resumed && m_coro) {
					m_asio_obj.cancel();
					m_error_code = boost::system::errc::make_error_code(boost::system::errc::timed_out);
					m_coro_resumed = true;
					m_coro.resume();
				}
			}

			bool				m_coro_resumed = false;
			ObjType				m_asio_obj;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_error_code;
			duration			m_timeout;
			asio_timer			m_timeout_timer;
		};

		template<ResumeIn thread_to_resume, typename ObjType, typename... Rs>
		class completion_or_timeout {
		public:
			using coordinator_ptr = std::shared_ptr<JobCoordinator<thread_to_resume, ObjType, Rs...>>;
			using async_task = std::function<void(coordinator_ptr&, ObjType&)>;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using duration = boost::posix_time::time_duration;
			using coro_handle = std::experimental::coroutine_handle<void>;

			completion_or_timeout(async_task&& work, ObjType& s, const duration& timeout) :
				m_job_coordinator(std::make_shared<JobCoordinator<thread_to_resume, ObjType, Rs...>>(std::move(work), s, timeout)) {}

			completion_or_timeout(completion_or_timeout&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_job_coordinator->start_work(coro, m_job_coordinator);
			}

			result_type await_resume() {
				return m_job_coordinator->get_result();
			}
		private:
			coordinator_ptr		m_job_coordinator;
		};

		template<ResumeIn thread_to_resume, typename ObjType, typename... Rs>
		class completion_or_timeout<thread_to_resume, ObjType&&, Rs...> {
		public:
			using coordinator_ptr = std::shared_ptr<JobCoordinator<thread_to_resume, ObjType&&, Rs...>>;
			using async_task = std::function<void(coordinator_ptr&, ObjType&)>;
			using result_type = std::tuple<boost::system::error_code, Rs...>;
			using duration = boost::posix_time::time_duration;
			using coro_handle = std::experimental::coroutine_handle<void>;

			completion_or_timeout(async_task&& work, ObjType&& s, const duration& timeout) :
				m_job_coordinator(std::make_shared<JobCoordinator<thread_to_resume, ObjType&&, Rs...>>(std::move(work), std::move(s), timeout)) {}

			completion_or_timeout(completion_or_timeout&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_job_coordinator->start_work(coro, m_job_coordinator);
			}

			result_type await_resume() {
				return m_job_coordinator->get_result();
			}
		private:
			coordinator_ptr		m_job_coordinator;
		};

		template<ResumeIn thread_to_resume, typename ObjType>
		class completion_or_timeout<thread_to_resume, ObjType> {
		public:
			using coordinator_ptr = std::shared_ptr<JobCoordinator<thread_to_resume, ObjType>>;
			using async_task = std::function<void(coordinator_ptr&, ObjType&)>;
			using result_type = boost::system::error_code;
			using duration = boost::posix_time::time_duration;
			using coro_handle = std::experimental::coroutine_handle<void>;

			completion_or_timeout(async_task&& work, ObjType& s, const duration& timeout) :
				m_job_coordinator(std::make_shared<JobCoordinator<thread_to_resume, ObjType>>(std::move(work), s, timeout)) {}

			completion_or_timeout(completion_or_timeout&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_job_coordinator->start_work(coro, m_job_coordinator);
			}

			result_type await_resume() {
				return m_job_coordinator->get_result();
			}
		private:
			coordinator_ptr		m_job_coordinator;
		};

		template<ResumeIn thread_to_resume, typename ObjType>
		class completion_or_timeout<thread_to_resume, ObjType&&> {
		public:
			using coordinator_ptr = std::shared_ptr<JobCoordinator<thread_to_resume, ObjType&&>>;
			using async_task = std::function<void(coordinator_ptr&, ObjType&)>;
			using result_type = boost::system::error_code;
			using duration = boost::posix_time::time_duration;
			using coro_handle = std::experimental::coroutine_handle<void>;

			completion_or_timeout(async_task&& work, ObjType&& s, const duration& timeout) :
				m_job_coordinator(std::make_shared<JobCoordinator<thread_to_resume, ObjType&&>>(std::move(work), std::move(s), timeout)) {}

			completion_or_timeout(completion_or_timeout&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_job_coordinator->start_work(coro, m_job_coordinator);
			}

			result_type await_resume() {
				return m_job_coordinator->get_result();
			}
		private:
			coordinator_ptr		m_job_coordinator;
		};

		template<typename ObjType, typename... Rs>
		class completion {
		public:
			using async_task = std::function<void(completion&, ObjType&)>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using result_type = std::tuple<boost::system::error_code, Rs...>;

			completion(async_task work, ObjType& s) :
				m_asio_obj_ref(s), m_async_work(move(work)) {}

			completion(completion&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_coro = coro;
				m_async_work(*this, m_asio_obj_ref);
			}

			result_type await_resume() {
				return m_result;
			}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				m_result = std::forward_as_tuple(ec, results...);
				if (m_coro)
					m_coro.resume();
			}
		private:
			ObjType & m_asio_obj_ref;
			async_task			m_async_work;
			coro_handle			m_coro;
			result_type			m_result;
		};

		template<typename ObjType, typename... Rs>
		class completion<ObjType&&, Rs...> {
		public:
			using async_task = std::function<void(completion&, ObjType&)>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using result_type = std::tuple<boost::system::error_code, Rs...>;

			completion(async_task work, ObjType&& s) :
				m_asio_obj(std::move(s)), m_async_work(move(work)) {}

			completion(completion&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_coro = coro;
				m_async_work(*this, m_asio_obj);
			}

			result_type await_resume() {
				return m_result;
			}

			template<typename... ATs>
			std::enable_if_t<(sizeof...(Rs) > 0) && sizeof...(ATs) == sizeof...(Rs)>
				done_with_result(boost::system::error_code ec, ATs&&... results) {
				m_result = std::forward_as_tuple(ec, results...);
				if (m_coro)
					m_coro.resume();
			}
		private:
			ObjType			m_asio_obj;
			async_task		m_async_work;
			coro_handle		m_coro;
			result_type		m_result;
		};

		template<typename ObjType>
		class completion<ObjType> {
		public:
			using async_task = std::function<void(completion&, ObjType&)>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using result_type = boost::system::error_code;

			completion(async_task work, ObjType& s) :
				m_asio_obj_ref(s), m_async_work(move(work)) {}

			completion(completion&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_coro = coro;
				m_async_work(*this, m_asio_obj_ref);
			}

			result_type await_resume() {
				return m_error_code;
			}

			void done_with_result(boost::system::error_code ec) {
				m_error_code = ec;
				if (m_coro)
					m_coro.resume();
			}
		private:
			ObjType & m_asio_obj_ref;
			async_task		m_async_work;
			coro_handle		m_coro;
			result_type		m_error_code;
		};

		template<typename ObjType>
		class completion<ObjType&&> {
		public:
			using async_task = std::function<void(completion&, ObjType&)>;
			using coro_handle = std::experimental::coroutine_handle<void>;
			using result_type = boost::system::error_code;

			completion(async_task work, ObjType&& s) :
				m_asio_obj(std::move(s)), m_async_work(move(work)) {}

			completion(completion&& other) = default;

			bool await_ready() { return false; }

			void await_suspend(coro_handle coro) {
				m_coro = coro;
				m_async_work(*this, m_asio_obj);
			}

			result_type await_resume() {
				return m_error_code;
			}

			void done_with_result(boost::system::error_code ec) {
				m_error_code = ec;
				if (m_coro)
					m_coro.resume();
			}
		private:
			ObjType			m_asio_obj;
			async_task		m_async_work;
			coro_handle		m_coro;
			result_type		m_error_code;
		};

		template<typename ObjType>
		using rw_completion = completion<ObjType, std::size_t>;

		template<ResumeIn thread_to_resume, typename ObjType>
		using rw_completion_or_timeout = completion_or_timeout<thread_to_resume, ObjType, std::size_t>;

		using tcp_socket = boost::asio::ip::tcp::socket;
		using udp_socket = boost::asio::ip::udp::socket;
		using tcp_endpoint = boost::asio::ip::tcp::endpoint;
		using udp_endpoint = boost::asio::ip::udp::endpoint;
		using tcp_acceptor = boost::asio::ip::tcp::acceptor;

		using deadline_timer = boost::asio::deadline_timer;
		using high_resolution_timer = boost::asio::high_resolution_timer;
		using system_timer = boost::asio::system_timer;
		using steady_timer = boost::asio::steady_timer;
	}

}
#endif