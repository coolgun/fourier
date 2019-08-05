#pragma once

#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
#include <execution>

namespace fourtd
{
	inline constexpr double pi = 3.1415926535897932385;
	using  complex_double = std::complex<double>;

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



			TrigonometricIterator(const complex_double &start_sincos, double start_angle = 0.0) :
				start_sincos(start_sincos),
				cur_sincos(make_sincos(start_angle))

			{

			}

			TrigonometricIterator(double delta_angle, double start_angle = 0.0) :
				start_sincos(make_sincos(delta_angle)),
				cur_sincos(make_sincos(start_angle))
			{

			}


			const complex_double sincos() const
			{
				return cur_sincos;
			}

			double cos() const
			{
				return cur_sincos.real();
			}

			double sin() const
			{
				return cur_sincos.imag();
			}

			const complex_double operator*() const
			{
				return sincos();
			}


			void step()
			{
				cur_sincos =
				{
					start_sincos.real() * cur_sincos.real() - start_sincos.imag() * cur_sincos.imag(),
					start_sincos.real() * cur_sincos.imag() + start_sincos.imag() * cur_sincos.real()

				};
			}


			TrigonometricIterator& operator++()
			{
				step();
				return *this;
			}

			TrigonometricIterator& operator+=(size_t step_count)
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

			const double  delta = pi / size;

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

				const auto val = nativ_value(c);
				std::get<0>(el) = std::abs(val - p0);
				std::get<1>(el) = c;
			}
			);

			const auto max_el = std::min_element(ranges.cbegin(), ranges.cend());

			const auto t = std::get<1>(*max_el);
			const auto val = nativ_value(t);
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

		template<class _FwdIt>
		explicit fourier(_FwdIt _First, _FwdIt _Last)
		{
			calcul_coeff(_First, _Last);
		}

		template<class C> static complex_double make_complex(C&& c);
		template<class C> static C make_value(const complex_double& z);
	

		template<class _FwdIt>
		void calcul_coeff(_FwdIt _First, _FwdIt _Last)
		{
			if (_First == _Last) return;
			
			size = std::distance(_First,_Last);
			is_odd = size % 2 != 0;

			ab.clear();
			a0 = {};
			complex_double bn;
			auto n = is_odd ? (size - 1) / 2 : size / 2 - 1;
			bool is_plus = true;

			const auto _UBFirst = std::_Get_unwrapped(_First);
			const auto _ULast = std::_Get_unwrapped(_Last);
			for (auto _UFirst=_UBFirst; _UFirst != _ULast; ++_UFirst)
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
			++it;

			while (n--)
			{
				ab.emplace_back(*it, complex_double{});
				++it;

			}

			std::for_each(std::execution::par, ab.begin(), ab.end(),
				[this, del, _UBFirst, _ULast](auto &el)
			{
				complex_double a, b;
				TrigonometricIterator it(el.first);
				++it;
				for (auto _UFirst = _UBFirst; _UFirst != _ULast; ++_UFirst)
				{
					const auto z = make_complex(*_UFirst);
					a += z * it.cos();
					b += z * it.sin();
					it += 2;

				}
				el = std::make_pair(a *del, b *del);

			}
			);

			if (!is_odd)
				ab.push_back({ {},bn });

		}

		complex_double operator () (double t) const
		{
			return value(t);
		}

		/*double length(double a, double b, double delta = 0.01) const
		{
			a = indexToAngle(a);
			b = indexToAngle(b);
			delta = 2 * delta*M_PI / size;

			for (double t = a; t < b; t += delta)
			{
				ret.push_back(simple_nativ_value(it.ccos(), it.csin()));
				++it;
			}


		}*/

		template<typename Fun> void values(Fun &&fun, double a, double b, double delta = 0.01) const
		{
			a = indexToAngle(a);
			b = indexToAngle(b);
			delta = 2 * delta*pi / size;

			TrigonometricIterator it(delta, a);
			++it;
		

			for (double t = a; t < b; t += delta, ++it)
			{
				fun(nativ_value(*it));
			}

		}

		template<typename C> std::vector<C> values(double a, double b, double delta = 0.01) const
		{
			std::vector<C> ret_vec;
			values([&ret_vec](const complex_double &v) 
					{ret_vec.emplace_back(make_value<C>(v));
					}, a, b, delta);
			return ret_vec;
		}


		complex_double value(double idx) const
		{
			return nativ_value(indexToAngle(idx));
		}


		static void  fun_step(complex_double &sum, const TrCoeff &c, const complex_double &sincos, size_t)
		{
			sum += c.first*sincos.real() + c.second*sincos.imag();
		}

		static void  derivative_step(complex_double &sum, const TrCoeff &c, const complex_double &sincos, size_t it_num)
		{
			sum += (-c.first*sincos.imag() + c.second*sincos.real())*static_cast<double>(it_num);
		}


		template <typename ... Funs>
		complex_double forEach(const complex_double &start, const complex_double &start_sincos, Funs&&... funs) const
		{
			auto  sum = start;

			TrigonometricIterator it(start_sincos, 0.0);
			++it;
			size_t k = 1;
			for (const auto &c : ab)
			{
				((funs(sum, c, *it, k)), ...);
				++it;
				++k;
			}
			return sum;

		}



		template <typename ... Funs>
		complex_double nativ_value(const complex_double &start_sincos, Funs&&... funs) const
		{
			return forEach(a0, start_sincos, std::forward<Funs>(funs)..., &fourier::fun_step);
		}


		template <typename ... Funs>
		complex_double nativ_value(double angle, Funs&&... funs) const
		{
			return nativ_value<Funs...>(make_sincos(angle), std::forward<Funs>(funs)...);

		}


	};


}

