#pragma once

#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
#include <execution>
#include <future>

namespace fourtd
{
	inline constexpr double pi = 3.1415926535897932385;
	using  complex_double = std::complex<double>;

	inline constexpr int MT_MIN_SIZE = 10'000;

	template <typename InputIt, typename T, typename Op>
	auto parallel_accumulate(InputIt first, InputIt last, T init, Op op)
	{
		const auto size = std::distance(first, last);
		
		if(size< MT_MIN_SIZE)
			return std::accumulate(first, last, T{}, op);

		const auto parts = std::thread::hardware_concurrency();

		std::vector<std::future<T>> futures;
		futures.reserve(parts);
		
		for (std::size_t i = 0; i != parts; ++i) 
		{
			const auto part_size = (size * i + size) / parts - (size * i) / parts;
			futures.emplace_back(std::async(std::launch::async,
				[=] {
					return std::accumulate(first, std::next(first, part_size), T{}, op); 
				}));
			std::advance(first, part_size);
		}

		
		return std::accumulate(std::begin(futures), std::end(futures), init,[](const T prev, auto& future) { return prev + future.get(); });
	}

	class fourier
	{

		static complex_double make_sincos(double start_angle)
		{
			return std::polar(1.0, start_angle);
		}


		struct TrigonometricIterator 
		{

			const complex_double start_sincos;
			complex_double cur_sincos;


			explicit TrigonometricIterator(const complex_double &start_sincos, double start_angle = 0.0) noexcept :
				start_sincos(start_sincos),
				cur_sincos(make_sincos(start_angle))

			{
				step();
			}

			explicit TrigonometricIterator(double delta_angle, double start_angle = 0.0) noexcept :
				start_sincos(make_sincos(delta_angle)),
				cur_sincos(make_sincos(start_angle))
			{
				step();
			}


			complex_double sincos() const noexcept
			{
				return cur_sincos;
			}

			double cos() const noexcept
			{
				return cur_sincos.real();
			}

			double sin() const noexcept
			{
				return cur_sincos.imag();
			}

			const complex_double &operator*() const noexcept
			{
				return cur_sincos;
			}

			const complex_double *operator->() const noexcept
			{
				return &cur_sincos;
			}


			void step() noexcept 
			{
				cur_sincos =
				{
					start_sincos.real() * cur_sincos.real() - start_sincos.imag() * cur_sincos.imag(),
					start_sincos.real() * cur_sincos.imag() + start_sincos.imag() * cur_sincos.real()

				};
			}


			TrigonometricIterator& operator++() noexcept
			{
				step();
				return *this;
			}

			TrigonometricIterator operator++(int) noexcept
			{
				TrigonometricIterator it(*this);
				step();
				return it;
			}

			TrigonometricIterator& operator+=(size_t step_count)noexcept
			{
				for (size_t i = 0; i < step_count; ++i)
					step();
				return *this;
			}


		};

		using TrCoeff = std::pair<complex_double, complex_double>;

		complex_double		 a0;
		std::vector<TrCoeff> ab;
		size_t size{};
		bool is_odd = {};
		mutable double square_value = -1.0;

	public:

		double norma(double t, const complex_double &p0) const
		{
			complex_double d;
			const auto val = nativ_value(t, [&d](complex_double&, const TrCoeff &c, const complex_double &sincos, size_t k)
			{
				derivative_step(d, c, sincos, k);
			}
			) - p0;
			return val.real()*d.real() + val.imag()*d.imag();

		}

		std::tuple<double, complex_double, double > lengthToPoint(const complex_double &p0,int &cnt) const
		{
			std::vector<std::tuple<double, double, double, double>>  ranges;

			const double  delta = 2*pi / size;

			for (size_t i = 0; i < size; ++i)
				ranges.emplace_back(0.0, std::numeric_limits<double>::max(), i*delta, (i + 1)*delta);

			std::atomic_int  call_fun_cnt{};

			std::for_each(std::execution::par, ranges.begin(), ranges.end(),
				[this, &p0,&call_fun_cnt](auto &el)
			{
				double a = std::get<2>(el);
				double b = std::get<3>(el);
				double norma_a = norma(a, p0);
				auto norma_c = norma_a;
				double c = -1;
				int l = 0;
				while (std::abs(norma_c) > 0.001 && std::abs(a - b) > 0.001)
				{
					c = (a + b) / 2.0;
					norma_c = norma(c, p0);
					++call_fun_cnt;
					if (norma_a * norma_c > 0)
					{
						a = c;
						norma_a = norma_c;
					}
					else
					{
						b = c;
					}
					++l;
				}

				const auto val = single_nativ_value(c);
				std::get<0>(el) = std::abs(val - p0);
				std::get<1>(el) = c;
			}
			);

			const auto max_el = std::min_element(ranges.cbegin(), ranges.cend());

			const auto t = std::get<1>(*max_el);
			const auto val = single_nativ_value(t);
			cnt = call_fun_cnt;

			return { t,val,std::abs(val - p0) };

		}

		double indexToAngle(double t) const
		{
			return (1 + 2 * t) * pi / size;
		}

		double angleToIndex(double t) const
		{
			return (t*size / pi - 1.0) / 2.0;
		}

		fourier() = delete;
		fourier(const fourier&) = delete;
		fourier(fourier&&) = delete;
		fourier& operator =(const fourier&) = delete;


		template<class _FwdIt>
		explicit fourier(_FwdIt _First, _FwdIt _Last)
		{
			calcul_coeff(_First, _Last);
		}

		template<class C> static complex_double make_complex(C&&c);
		template<class C> static C make_value(const complex_double& z);


		double simpson(double a, double b, size_t size)  const
		{


			const double delta = (b - a) / size;
			const auto parts = 1;// std::thread::hardware_concurrency();
			const double d2 = (b - a) / parts;

			std::vector<std::future<double>> futures;
			futures.reserve(parts);

			for (std::size_t i = 0; i != parts; ++i)
			{
				auto al = a + i * d2;
				auto bl = a + (i+1) * d2;

				futures.emplace_back(std::async(std::launch::async,
					[this, al , bl, delta]
				{
					double f1 = 0.0, f2 = 0.0;
					TrigonometricIterator it(delta, al);
					const double fs = std::abs(derivative_value(*it));
					++it;

					for (double t = al + delta; t < bl; )
					{
						f1 += std::abs(derivative_value(*it));
						t += delta; ++it;
						f2 += std::abs(derivative_value(*it));
						t += delta; ++it;
					}

					return delta / 3 * (fs + 4 * f1 + 2 * f2);
				})
				);
			}


			return std::accumulate(futures.begin(), futures.end(), 0.0, [](const auto prev, auto& future) { return prev + future.get(); });
		}

		double length(double a, double b, double eps = 0.1) const
		{
			
			if (ab.empty()) return {};
			a = indexToAngle(a);
			b = indexToAngle(b);
			
			size_t n = 20; 

			double first  = simpson(a, b, n); 
			double second{};

			do 
			{
				second = first;
				n*=2;  
				first = simpson(a, b, n);
			} 
			while (std::abs(first - second) > eps);
			return first;
		}


		double square()const
		{
			if (square_value<0.0)
			{
				square_value = pi * std::abs(std::accumulate
				(
					ab.cbegin()
					, ab.cend()
					, 0.0
					, [](double square_value, const TrCoeff &c)
					{
						return square_value + c.first.real()*c.second.imag() - c.first.imag()*c.second.real();
					})
				);
			}
			
			return square_value;
		}
	
		template<class C>	bool containsPoint(C&& c)
		{
			const auto z0 = make_complex(std::forward<C>(c));
		//	auto a = std::log((nativ_value(2 * pi) - z0)- std::log((nativ_value(2 * pi) - z0)
			
			const auto val = std::log(nativ_value(pi) - z0) - std::log(nativ_value(0) - z0);
			const auto val1 = std::log(nativ_value(2 * pi) - z0) - std::log(nativ_value(pi) - z0);

			return std::abs(val-val1) != 0;

		}

		template<class _FwdIt>
		void calcul_coeff(_FwdIt _First, _FwdIt _Last)
		{
			ab.clear();
			square_value = -1.0; //reset;
			if (_First == _Last) return;
			
			size = std::distance(_First,_Last);
			is_odd = size % 2 != 0;
			
			a0 = {};
			complex_double bn;
			
			bool is_plus = true;

			const auto _UBFirst = std::_Get_unwrapped(_First);
			const auto _ULast = std::_Get_unwrapped(_Last);
			for (auto _UFirst = _UBFirst; _UFirst != _ULast; ++_UFirst)
			{
				const auto z = make_complex(*_UFirst);
				a0 += z;
				bn += (is_plus ? z : -z);
				is_plus = !is_plus;
			}

			a0 /= static_cast<double>(size);
			bn /= static_cast<double>(size);

			const auto del = 2.0 / static_cast<double>(size);
			const auto d_angle = pi / static_cast<double>(size);

			TrigonometricIterator it(d_angle);
			
			for (auto n = is_odd ? (size - 1) / 2 : size / 2 - 1; n--; ++it)
			{
				ab.emplace_back(*it, complex_double{});
			}

			std::for_each(std::execution::par, ab.begin(), ab.end(),[this, del, _UBFirst, _ULast](auto &el)
			{
				complex_double a, b;
				TrigonometricIterator it(el.first);
				
				for (auto _UFirst = _UBFirst; _UFirst != _ULast; ++_UFirst, it += 2)
				{
					const auto z = make_complex(*_UFirst);
					a += z * it.cos();
					b += z * it.sin();
				}
				el.first = a * del;
				el.second = b * del;
			}
			);

			if (!is_odd)
				ab.emplace_back(complex_double{},bn);

		}

		complex_double operator () (double t) const
		{
			return value(t);
		}



		template<typename GetFun, typename CalculFun> void values_impl(GetFun &&get_fun, CalculFun &&calc_fun, double a, double b, double delta) const
		{
			a = indexToAngle(a);
			b = indexToAngle(b);
			delta = 2 * delta*pi / size;

			TrigonometricIterator it(delta, a);
		
			for (double t = a; t < b; t += delta, ++it)
			{
				get_fun(calc_fun(*it));
			}
		}


		template<typename C, typename OutIt> void values(OutIt it,double a, double b, double delta = 0.01) const
		{

			values_impl
			(
				[&it](const complex_double &v){*it=make_value<C>(v);++it;}
				,std::bind(&fourier::single_nativ_value,this, std::placeholders::_1)
				,a
				,b
				,delta
			);
		}


		complex_double value(double idx) const
		{
			return single_nativ_value(indexToAngle(idx));
		}


		static void  fun_step(complex_double &sum, const TrCoeff &c, const complex_double &sincos, size_t)
		{
			sum += c.first*sincos.real() + c.second*sincos.imag();
		}

		static void  derivative_step(complex_double &sum, const TrCoeff &c, const complex_double &sincos, size_t it_num)
		{
			sum += (-c.first*sincos.imag() + c.second*sincos.real())*static_cast<double>(it_num);
		}


		template <typename MainFun, typename ... Funs>
		complex_double forEach(const complex_double &start, const complex_double &start_sincos, MainFun &&main_fun,Funs &&... funs) const
		{
			auto  sum = start;

			TrigonometricIterator it(start_sincos, 0.0);
			size_t k = 1;
			for (const auto &c : ab)
			{
				((funs(sum, c, *it, k)), ...);
				main_fun(sum, c, *it, k);
				++it;
				++k;
			}
			return sum;

		}


		template <typename ... Funs>
		complex_double derivative_value(const complex_double &start_sincos, Funs&&... funs) const
		{
			return forEach({}, start_sincos, &fourier::derivative_step, std::decay_t<Funs>(std::forward<Funs>(funs))...);
		}

	
		template <typename ... Funs>
		complex_double nativ_value(const complex_double &start_sincos,Funs&&... funs) const
		{
			return forEach(a0, start_sincos, &fourier::fun_step, std::decay_t<Funs>(std::forward<Funs>(funs))...);
		}

		complex_double single_nativ_value(const complex_double &start_sincos) const
		{
			return nativ_value(start_sincos);
		}

		template <typename ... Funs>
		complex_double nativ_value(double angle,Funs&&... funs) const
		{
			return nativ_value<Funs...>(make_sincos(angle),std::decay_t<Funs>(std::forward<Funs>(funs))...);
		}



	};


}

