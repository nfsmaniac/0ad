#include "precompiled.h"

#include "EntitySupport.h"

bool CClassSet::IsMember( const CStrW& Test )
{
	for(size_t i = 0; i < m_Members.size(); i++)
	{
		if(m_Members[i] == Test)
		{
			// Move to Front algo (currently disabled)
			//std::swap(m_Members[0], m_Members[i]);

			return true;
		}
	}
	return false;
}

void CClassSet::Rebuild()
{
	if( m_Parent )
		m_Members = m_Parent->m_Members;
	else
		m_Members.clear();
	
	Set newMembers;	
	
	for( size_t i=0; i<m_Members.size(); ++i )
	{
		const CStrW& member = m_Members[i];
		
		for(size_t j=0; j<m_RemovedMembers.size(); j++) 
		{
			if(m_RemovedMembers[j] == member)
				goto end;
		}
		
		for(size_t j=0; j<m_AddedMembers.size(); j++) 
		{
			if(m_AddedMembers[j] == member)
				goto end;
		}
		
		newMembers.push_back(member);
		
		end:;
	}
	
	for(size_t j=0; j<m_AddedMembers.size(); j++) 
	{
		newMembers.push_back(m_AddedMembers[j]);
	}
	
	m_Members = newMembers;
}

CStrW CClassSet::getMemberList()
{
	Set::iterator it = m_Members.begin();
	CStrW result = L"";
	if( it != m_Members.end() )
	{
		result = *( it++ );
		for( ; it != m_Members.end(); ++it )
			result += L" " + *it;
	}
	return result;
}

void CClassSet::setFromMemberList(const CStrW& list)
{
	CStr entry;
	CStr temp = list;

	m_AddedMembers.clear();
	m_RemovedMembers.clear();

	while( true )
	{
		// Find the first ' ' or ',' in the string
		int brk = temp.Find(' ');
		int comma = temp.Find(',');
		if( comma != -1  && ( brk == -1 || comma < brk ) )
		{
			brk = comma;
		}

		if( brk == -1 )
		{
			entry = temp;
		}
		else
		{
			entry = temp.substr( 0, brk );
			temp = temp.substr( brk + 1 );
		}

		if( brk != 0 )
		{
			if( entry[0] == '-' )
			{
				entry = entry.substr( 1 );
				if( ! entry.empty() )
					m_RemovedMembers.push_back( entry );
			}
			else
			{
				if( entry[0] == '+' )
					entry = entry.substr( 1 );
				if( ! entry.empty() )
					m_AddedMembers.push_back( entry );
			}
		}
		if( brk == -1 )
		{
			break;
		}
	}

}


