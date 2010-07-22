/*
Copyright 2010 SourceGear, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <sg.h>

void SG_dbcriteria__check_one_record(SG_context* pCtx, const SG_varray* pcrit, SG_dbrecord* prec, SG_bool* pbResult)
{
	const char* psz_op = NULL;

	SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_OP, &psz_op)  );

	if (
		(0 == strcmp("<", psz_op))
		|| (0 == strcmp(">", psz_op))
		|| (0 == strcmp("<=", psz_op))
		|| (0 == strcmp(">=", psz_op))
		|| (0 == strcmp("!=", psz_op))
		)
	{
		const char* psz_left = NULL;
		SG_int64 iVal;
		SG_int64 iRecVal;
		const char* putf8RecValue;

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

		SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &iVal)  );

		SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, psz_left, &putf8RecValue)  );

		SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &iRecVal, putf8RecValue)  );

		if (0 == strcmp("<", psz_op))
		{
			*pbResult = (iRecVal <  iVal);
		}
		else if (0 == strcmp(">", psz_op))
		{
			*pbResult = (iRecVal >  iVal);
		}
		else if (0 == strcmp("<=", psz_op))
		{
			*pbResult = (iRecVal <=  iVal);
		}
		else if (0 == strcmp(">=", psz_op))
		{
			*pbResult = (iRecVal >=  iVal);
		}
		else if (0 == strcmp("!=", psz_op))
		{
			*pbResult = (iRecVal !=  iVal);
		}
		else
		{
			SG_ERR_THROW_RETURN(SG_ERR_INVALID_DBCRITERIA);
		}

		return;
	}
	else if (0 == strcmp("==", psz_op))
	{
		const char* psz_left = NULL;
		SG_uint16 t;

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

		SG_ERR_CHECK(  SG_varray__typeof(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &t)  );

		if (t == SG_VARIANT_TYPE_INT64)
		{
			SG_int64 iVal;
			SG_int64 iRecVal;
			const char* putf8RecValue;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

			SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &iVal)  );

			SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, psz_left, &putf8RecValue)  );

			SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &iRecVal, putf8RecValue)  );

			*pbResult = (iRecVal ==  iVal);

			return;
		}
		else if (t == SG_VARIANT_TYPE_SZ)
		{
			const char* psz_right = NULL;
			const char* putf8RecValue = NULL;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &psz_right)  );

			SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, psz_left, &putf8RecValue)  );

			if (0 == SG_utf8__compare(psz_right, putf8RecValue))
			{
				*pbResult = SG_TRUE;
			}
			else
			{
				*pbResult = SG_FALSE;
			}

			return;
		}
		else
		{
			SG_ERR_THROW_RETURN(SG_ERR_INVALID_DBCRITERIA);
		}
	}
	else if (0 == strcmp("exists", psz_op))
	{
		const char* psz_left = NULL;

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

		SG_ERR_CHECK_RETURN(  SG_dbrecord__has_value(pCtx, prec, psz_left, pbResult)  );
		return;
	}
	else if (0 == strcmp("&&", psz_op))
	{
		SG_varray* pcrit_left = NULL;
		SG_varray* pcrit_right = NULL;
		SG_bool b;

		SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_LEFT, &pcrit_left)  );

		SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &pcrit_right)  );

		SG_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit_left, prec, &b)  );

		if (!b)
		{
			*pbResult = SG_FALSE;
			return;
		}

		SG_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit_right, prec, &b)  );

		*pbResult = b;
		return;
	}
	else if (0 == strcmp("||", psz_op))
	{
		SG_varray* pcrit_left = NULL;
		SG_varray* pcrit_right = NULL;
		SG_bool b;

		SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_LEFT, &pcrit_left)  );

		SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &pcrit_right)  );

		SG_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit_left, prec, &b)  );

		if (b)
		{
			*pbResult = SG_TRUE;
			return;
		}

		SG_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit_right, prec, &b)  );

		*pbResult = b;
		return;
	}
	else
	{
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_DBCRITERIA);
	}

	// NOTREACHED -- SG_ASSERT(0);

fail:
	return;
}


