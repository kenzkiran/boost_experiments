

auto executor = pool.get_executor();
auto work_guard = boost::asio::make_work_guard(executor);
boost::asio::post(pool, [&] {
  std::cout << "Ravi inside task\n";
  boost::asio::post(pool, [&] {
    std::cout << "Ravi2 inside task\n";
    work_guard.reset();
  });
});
pool.wait();