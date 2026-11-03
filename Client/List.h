#pragma once
#include <cassert>
#include <initializer_list>

template <typename T>
class List
{
public:
	struct Node
	{
		T _data;
		Node* _prev;
		Node* _next;
	};
	class Iterator
	{
	private:
		friend class List;
		Node* _node;
	public:
		Iterator(Node* node = nullptr) : _node(node) {}

		const Iterator operator++(int)
		{
			Iterator ret = _node;
			_node = _node->_next;
			return ret;
		}
		
		Iterator& operator++()
		{
			_node = _node->_next;
			return *this;
		}

		Iterator operator--(int)
		{
			Iterator ret = _node;
			_node = _node->_prev;
			return ret;
		}

		Iterator& operator--()
		{
			_node = _node->_prev;
			return *this;
		}

		T& operator*() const
		{
			return _node->_data;
		}
		
		bool operator==(const Iterator& other) const
		{
			return _node == other._node;
		}

		bool operator!=(const Iterator& other) const
		{
			return _node != other._node;
		}
	};
	class ConstIterator
	{
	private:
		friend class List;
		const Node* _node;
	public:
		ConstIterator(const Node* node = nullptr) : _node(node) {}

		const ConstIterator operator++(int)
		{
			ConstIterator ret = _node;
			_node = _node->_next;
			return ret;
		}

		ConstIterator& operator++()
		{
			_node = _node->_next;
			return *this;
		}

		ConstIterator operator--(int)
		{
			ConstIterator ret = _node;
			_node = _node->_prev;
			return ret;
		}

		ConstIterator& operator--()
		{
			_node = _node->_prev;
			return *this;
		}

		const T& operator*() const
		{
			return _node->_data;
		}

		bool operator==(const ConstIterator& other) const
		{
			return _node == other._node;
		}

		bool operator!=(const ConstIterator& other) const
		{
			return _node != other._node;
		}
	};

public:
	List()
	{
		_head._next = &_tail;
		_tail._prev = &_head;
	}

	List(const List<T>& src)
	{
		_head._next = &_tail;
		_tail._prev = &_head;

		for (auto it = src.begin(); it != src.end(); ++it)
		{
			push_back(*it);
		}
	}
	List<T>& operator=(const List<T>& src)
	{
		if (this == &src)
			return *this;

		clear();

		for (auto it = src.begin(); it != src.end(); ++it)
		{
			push_back(*it);
		}

		return *this;
	}

	List(List<T>&& src) noexcept
	{
		if (src.empty())
		{
			_head._next = &_tail;
			_tail._prev = &_head;
			return;
		}

		// src에서 노드들 옮겨오기.
		_head._next = src._head._next;
		_tail._prev = src._tail._prev;
		_head._next->_prev = &_head;
		_tail._prev->_next = &_tail;
		_size = src._size;

		// src 초기화
		src._head._next = &(src._tail);
		src._tail._prev = &(src._head);
		src._size = 0;
	}
	List<T>& operator=(List<T>&& src) noexcept
	{
		if (this == &src)
			return *this;

		clear();

		if (src.empty())
		{
			_head._next = &_tail;
			_tail._prev = &_head;
			return *this;
		}

		// src에서 노드들 옮겨오기.
		_head._next = src._head._next;
		_tail._prev = src._tail._prev;
		_head._next->_prev = &_head;
		_tail._prev->_next = &_tail;
		_size = src._size;

		// src 초기화
		src._head._next = &(src._tail);
		src._tail._prev = &(src._head);
		src._size = 0;

		return *this;
	}

	List(Iterator first, Iterator last)
	{
		_head._next = &_tail;
		_tail._prev = &_head;

		for (first; first != last; ++first)
		{
			push_back(*first);
		}
	}
	List(std::initializer_list<T> iList)
	{
		_head._next = &_tail;
		_tail._prev = &_head;

		for (auto it = iList.begin(); it != iList.end(); ++it)
		{
			push_back(*it);
		}
	}
	List(size_t count, const T value)
	{
		_head._next = &_tail;
		_tail._prev = &_head;

		for (size_t i = 0; i < count; i++)
		{
			push_back(value);
		}
	}

	~List()
	{
		for (Iterator it = begin(); it != end();)
		{
			it = erase(it);
		}
	}

	Iterator begin()
	{
		return Iterator{ _head._next };
	}

	ConstIterator begin() const
	{
		return ConstIterator{ _head._next };
	}

	Iterator end()
	{
		return Iterator{ &_tail	};
	}

	ConstIterator end() const
	{
		return ConstIterator{ &_tail };
	}

	void push_front(const T data)
	{
		Node* newNode = new Node;
		newNode->_data = data;

		// newNode 본인 연결
		newNode->_next = _head._next;
		newNode->_prev = &_head;

		// newNode 앞 연결
		_head._next->_prev = newNode;
		
		// newNode 이전 연결
		_head._next = newNode;

		_size++;
	}

	void push_back(const T data)
	{
		Node* newNode = new Node;
		newNode->_data = data;

		// newNode 본인 연결
		newNode->_next = &_tail;
		newNode->_prev = _tail._prev;

		// newNode 앞 연결
		_tail._prev->_next = newNode;

		// newNode 이전 연결
		_tail._prev = newNode;

		_size++;
	}

	void pop_front()
	{
		assert(_size > 0 && "pop_front() called on empty list.");

		// 삭제할 노드
		Node* deleteNode = _head._next;

		// 주변 작업
		deleteNode->_next->_prev = &_head;
		_head._next = deleteNode->_next;

		// 삭제
		delete deleteNode;
		_size--;
	}

	void pop_back()
	{
		assert(_size > 0 && "pop_back() called on empty list.");

		// 삭제할 노드
		Node* deleteNode = _tail._prev;

		// 주변 작업
		deleteNode->_prev->_next = &_tail;
		_tail._prev = deleteNode->_prev;

		// 삭제
		delete deleteNode;
		_size--;
	}

	T& front()
	{
		assert(_size > 0 && "front() called on empty list.");
		return _head._next->_data;
	}
	
	const T& front() const
	{
		assert(_size > 0 && "front() called on empty list.");
		return _head._next->_data;
	}

	T& back()
	{
		assert(_size > 0 && "back() called on empty list.");
		return _tail._prev->_data;
	}

	const T& back() const
	{
		assert(_size > 0 && "back() called on empty list.");
		return _tail._prev->_data;
	}

	void clear()
	{
		for (Iterator it = begin(); it != end();)
		{
			it = erase(it);
		}
		_head._next = &_tail;
		_tail._prev = &_head;
		_size = 0;
	}

	size_t size() const { return _size; }

	bool empty() const { return _size == 0; }

	Iterator insert(Iterator where, T value)
	{
		// 추가할 노드 작업.
		Node* newNode = new Node;
		newNode->_data = value;
		newNode->_next = where._node;
		newNode->_prev = where._node->_prev;
		
		// 리턴용
		Iterator ret = newNode;

		// 주변 노드 작업
		where._node->_prev->_next = newNode;
		where._node->_prev = newNode;

		_size++;

		return ret;
	}

	Iterator insert(Iterator where, size_t count, T value)
	{
		Iterator ret;
		for (size_t i = 0; i < count; i++)
		{
			ret = insert(where, value);
		}
		return ret;
	}

	Iterator insert(Iterator where, std::initializer_list<T> iList)
	{
		Iterator ret;
		for (auto it = iList.begin(); it != iList.end(); ++it)
		{
			ret = insert(where, *it);
		}
		return ret;
	}

	Iterator insert(Iterator where, Iterator first, Iterator last)
	{
		/////////////////////////////////////////////////////////////////
		// 
		// self-insertion 동작 과정. (where가 first ~ last 안에 있는 경우)
		// 
		// offset: 0 1 2 3 4
		//         ↓ first   ↓ last
		//         1 2 3 4 5 -
		//             ↑ where = [offset 2]
		// 
		// 첫 번째 삽입.
		//           ↓ first     ↓ last
		//         1 2 [1] 3 4 5 -
		//    insertion ↑  ↑ where (삽입 이후에도 같은 노드를 계속 가리킴)
		// 
		// 두 번째 삽입.
		//				  ↓ first가 삽입한 노드 개수 + 1 만큼 한번에 건너뜀. (where의 offset 계산을 한 이유)
		//           | ↔↔↔↔↔↔↔ ↓ first  ↓ last
		//         1 2 [1] [2] 3  4  5  -
		//                     ↑ where
		// 
		// 세 번째 삽입.
		//                             ↓ first ↓ last
		//         1 2 [1] [2] [3] 3   4   5   -
		//                         ↑ where
		// 
		// 최종 결과.
		//                                       ↓ (first == last) 반복문 종료.
		//         1 2 [1] [2] [3] [4] [5] 3 4 5 -
		//                                 ↑ where
		/////////////////////////////////////////////////////////////////

		// self-insertion : where가 first ~ last 안에 있는 경우 where위치 offset 계산.
		Iterator temp = first;
		int offset = 0;
		int count = 0;
		for (; first != last; ++first)
		{
			if (where == first)
				offset = count; // where가 first ~ last 안에 있는 경우 그 위치(offset)을 계산

			count++; // 오프셋 계산을 위해 first -> last 이동한 횟수로 쓰임.
		}

		count = 0;
		first = temp;
		
		Iterator ret;
		for (; first != last;)
		{
			ret = insert(where, *first);
			count++; // 삽입한 횟수로 쓰임.

			// first가 건너뛰어야 할 때 건너뛰기.
			if (count == offset)
			{
				for (int i = 0; i <= offset; i++)
				{
					++first;
				}
			}
			else 
			{
				++first;
			}
		}
		return ret;
	}

	Iterator erase(Iterator where)
	{

		// 리턴용
		Iterator ret = where._node->_next;

		// 주변 노드 작업
		where._node->_prev->_next = where._node->_next;
		where._node->_next->_prev = where._node->_prev;

		// 본인 삭제
		delete where._node;
		_size--;

		return ret;
	}

	Iterator erase(Iterator first, Iterator last)
	{
		for (first; first != last;)
		{
			first = erase(first);
		}
		return first;
	}

	void remove(const T& data)
	{
		for (Iterator it = begin(); it != end(); ++it)
		{
			if (*it == data)
			{
				erase(it);
				return;
			}
		}
	}

	// 디버깅용
	void print()
	{
		if (empty())
		{
			std::cout << "list empty : " << std::boolalpha << empty() << "\n";
			std::cout << "list size : " << size() << "\n";
			std::cout << "\n\n";
			return;
		}

		std::cout << "list empty : " << std::boolalpha << empty() << "\n";
		std::cout << "list size : " << size() << "\n";
		std::cout << "list front : " << front() << "\n";
		std::cout << "list back : " << back() << "\n";
		for (auto it = begin(); it != end(); ++it)
		{
			std::cout << *it << " ";
		}
		std::cout << "\n\n";
	}

private:
	size_t _size = 0;
	Node _head;
	Node _tail;
};