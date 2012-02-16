/* @@@LICENSE
*
*      Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
LICENSE@@@ */

#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include <japi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct list_head {
	struct list_head *next, *prev;
};

typedef struct list_head list_head;

#define LIST_HEAD_INIT(name) ((struct list_head) { &(name), &(name) })
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
#define INIT_LIST_HEAD(ptr) do { (ptr)->next = (ptr); (ptr)->prev = (ptr); } while (0)


static inline void __ladd(list_head *new_item, list_head *prev, list_head *next)
{
	next->prev = new_item;
	new_item->next = next;
	new_item->prev = prev;
	prev->next = new_item;
}

static inline void ladd(list_head *new_list, list_head *head)
{
	__ladd(new_list, head, head->next);
}

static inline void ladd_tail(list_head *new_list, list_head *head)
{
	__ladd(new_list, head->prev, head);
}

static inline void __ldel(list_head *prev, list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void ldel(list_head *entry)
{
	__ldel(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

static inline bool list_empty(list_head *head)
{
	return head->next == head;
}

#define lentry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* LINKED_LIST_H_ */
