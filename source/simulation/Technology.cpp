#include "precompiled.h"
#include "Technology.h"
#include "TechnologyCollection.h"
#include "EntityManager.h"
#include "ps/CStr.h"
#include "ps/CLogger.h"
#include "scripting/ScriptingHost.h"
#include "ps/XML/Xeromyces.h"
#include "ps/XML/XeroXMB.h"
#include "Entity.h"
#include "EntityTemplate.h"
#include "EntityTemplateCollection.h"
#include "ps/Player.h"
#include "scripting/ScriptableComplex.inl"

#define LOG_CATEGORY "Techs"

STL_HASH_SET<CStr, CStr_hash_compare> CTechnology::m_scriptsLoaded;

CTechnology::CTechnology( const CStrW& name, CPlayer* player ) : m_Name(name), m_player(player)
{
	ONCE( ScriptingInit(); );

	m_researched=false;
	m_excluded = false;
	m_inProgress = false;
} 
bool CTechnology::loadXML( const CStr& filename )
{
	CXeromyces XeroFile;

	if (XeroFile.Load(filename) != PSRETURN_OK)
        return false;

	#define EL(x) int el_##x = XeroFile.getElementID(#x)

	EL(tech);
	EL(id);
	EL(req);
	EL(effect);
	
	#undef EL

	XMBElement Root = XeroFile.getRoot();
	if ( Root.getNodeName() != el_tech )
	{
		LOG( ERROR, LOG_CATEGORY, "CTechnology: XML root was not \"Tech\" in file %s. Load failed.", filename.c_str() );
		return false;
	}
	XMBElementList RootChildren = Root.getChildNodes();
	bool ret;
	for  ( int i=0; i<RootChildren.Count; ++i )
	{
		XMBElement element = RootChildren.item(i);
		int name = element.getNodeName();
		if ( name == el_id )
			ret = loadELID( element, XeroFile );
		else if ( name == el_req )
			ret = loadELReq( element, XeroFile );
		else if ( name == el_effect )
			ret = loadELEffect( element, XeroFile, filename );
		else 
			continue;
		if ( !ret )
		{
			LOG( ERROR, LOG_CATEGORY, "CTechnology: Load failed for file %s", filename.c_str() );
			return false;
		}
	}
	
	return true;	
}
bool CTechnology::loadELID( XMBElement ID, CXeromyces& XeroFile )
{
	#define EL(x) int el_##x = XeroFile.getElementID(#x)
	
	EL(generic);
	EL(specific);
	EL(icon);
	EL(icon_cell);
	EL(classes);
	EL(rollover);
	EL(history);

	#undef EL

	XMBElementList children = ID.getChildNodes();
	for ( int i=0; i<children.Count; ++i )
	{
		XMBElement element = children.item(i);
		int name = element.getNodeName();
		CStrW value = element.getText();
		
		if ( name == el_generic )
			m_Generic = value;
		else if ( name == el_specific )
			m_Specific = value;
		else if ( name == el_icon )
			m_Icon = value;
		else if ( name == el_icon_cell )
			m_IconCell = value.ToInt();
		else if ( name == el_classes )
			m_Classes = value;
		else if ( name == el_rollover )
			continue;	
		else if ( name == el_history )
			m_History = value;
		else
		{
			const char* tagName = XeroFile.getElementString(name).c_str();
			LOG( ERROR, LOG_CATEGORY, "CTechnology: invalid tag %s for XML file", tagName );
			return false;
		}
	}
	return true;
}
bool CTechnology::loadELReq( XMBElement Req, CXeromyces& XeroFile )
{
	#define EL(x) int el_##x = XeroFile.getElementID(#x)
	
	EL(time);
	EL(resource);
	EL(food);
	EL(tech);
	EL(stone);
	EL(metal);
	EL(wood);
	EL(entity);
	
	#undef EL

	XMBElementList children = Req.getChildNodes();
	for ( int i=0; i<children.Count; ++i )
	{
		XMBElement element = children.item(i);
		int name = element.getNodeName();
		CStrW value = element.getText();
		
		if ( name == el_time )
			m_ReqTime = value.ToFloat();
		else if ( name == el_resource )
		{
			XMBElementList resChildren = element.getChildNodes();
			for ( int j=0; j<resChildren.Count; ++j )
			{
				XMBElement resElement = resChildren.item(j);
				int resName = resElement.getNodeName();
				CStrW resValue = resElement.getText();

				if ( resName == el_food )	//NOT LOADED-GET CHILD NODES
					m_ReqFood = resValue.ToFloat();
				else if ( resName == el_wood )
					m_ReqWood = resValue.ToFloat();
				else if ( resName == el_stone )
					m_ReqStone = resValue.ToFloat();
				else if ( resName == el_metal )
					m_ReqMetal = resValue.ToFloat();
				else
				{
					const char* tagName = XeroFile.getElementString(name).c_str();
					LOG( ERROR, LOG_CATEGORY, "CTechnology: invalid tag %s for XML file", tagName );
					return false;
				}
			}
		}
		else if ( name == el_entity )
			m_ReqEntities.push_back( value );
		else if ( name == el_tech )
			m_ReqTechs.push_back( value );
		else
		{
			const char* tagName = XeroFile.getElementString(name).c_str();
			LOG( ERROR, LOG_CATEGORY, "CTechnology: invalid tag %s for XML file", tagName );
			return false;
		}
	}
	return true;
}
bool CTechnology::loadELEffect( XMBElement effect, CXeromyces& XeroFile, const CStr& filename )
{
	#define EL(x) int el_##x = XeroFile.getElementID(#x)
	#define AT(x) int at_##x = XeroFile.getAttributeID(#x)

	EL(target);
	EL(pair);
	EL(modifier);
	EL(attribute);
	EL(value);
	EL(set);
	EL(script);
	EL(function);
	AT(name);
	AT(file);
	
	#undef EL
	#undef AT

	XMBElementList children = effect.getChildNodes();
	for ( int i=0; i<children.Count; ++i )
	{
		XMBElement element = children.item(i);
		int name = element.getNodeName();
		CStr value = element.getText();

		if ( name == el_target )
			m_Targets.push_back(value);
		else if ( name == el_pair )
			m_Pairs.push_back(value);
		
		else if ( name == el_modifier )
		{
			XMBElementList modChildren = element.getChildNodes();
			m_Modifiers.push_back(Modifier());
			for ( int j=0; j<modChildren.Count; ++j )
			{
				XMBElement modElement = modChildren.item(j);
				CStrW modValue = modElement.getText();

				if ( modElement.getNodeName() == el_attribute)
					m_Modifiers.back().attribute = modValue;
				else if ( modElement.getNodeName() == el_value )
				{
					if( modValue.size() == 0)
					{
						LOG( ERROR, LOG_CATEGORY, "CTechnology::loadXML invalid Modifier value (empty string)" );
						m_Modifiers.pop_back();
						return false;
					}

					if( modValue[modValue.size()-1] == '%' )
					{
						m_Modifiers.back().isPercent = true;
						modValue = modValue.substr( 0, modValue.size()-1 );
					}
					m_Modifiers.back().value = modValue.ToFloat();
				}
				else
				{
					LOG( ERROR, LOG_CATEGORY, "CTechnology::loadXML invalid tag inside \"Modifier\" tag" );
					m_Modifiers.pop_back();
					return false;
				}
			}
		}

		else if ( name == el_set )
		{
			XMBElementList setChildren = element.getChildNodes();
			m_Sets.push_back(Modifier());
			for ( int j=0; j<setChildren.Count; ++j )
			{
				XMBElement setElement = setChildren.item(j);
				CStrW setValue = setElement.getText();

				if ( setElement.getNodeName() == el_attribute)
					m_Sets.back().attribute = setValue;
				else if ( setElement.getNodeName() == el_value )
					m_Sets.back().value = setValue.ToFloat();
				else
				{
					LOG( ERROR, LOG_CATEGORY, "CTechnology::loadXML invalid tag inside \"Set\" tag" );
					m_Sets.pop_back();
					return false;
				}
			}
		}
		
		else if ( name == el_script )
		{
			CStr Include = element.getAttributes().getNamedItem( at_file );
			if( !Include.empty() && m_scriptsLoaded.find( Include ) == m_scriptsLoaded.end() )
			{
				m_scriptsLoaded.insert( Include );
				g_ScriptingHost.RunScript( Include );
			}
			CStr Inline = element.getText();
			if( !Inline.empty() )
			{
				g_ScriptingHost.RunMemScript( Inline.c_str(), Inline.length(), filename, element.getLineNumber() );
			}
		}
		else if ( name == el_function )
		{
			utf16string funcName = element.getAttributes().getNamedItem( at_name );
			CStr Inline = element.getText();

			if ( funcName != utf16string() )
			{
				jsval fnval;
				JSBool ret = JS_GetUCProperty( g_ScriptingHost.GetContext(), g_ScriptingHost.GetGlobalObject(), funcName.c_str(), funcName.size(), &fnval );
				debug_assert( ret );
				JSFunction* fn = JS_ValueToFunction( g_ScriptingHost.GetContext(), fnval );
				if( !fn )
				{
					LOG( ERROR, LOG_CATEGORY, "CTechnology::LoadXML: Function does not exist for %hs in file %s. Load failed.", funcName.c_str(), filename.c_str() );
					return false;
				}
				m_effectFunction.SetFunction( fn );
			}
			else if ( Inline != CStr() )
				m_effectFunction.Compile( CStrW( filename ) + L"::" + (CStrW)funcName + L" (" + CStrW( element.getLineNumber() ) + L")", Inline );
			//(No error needed; scripts are optional)
		}
		else
		{
			const char* tagName = XeroFile.getElementString(name).c_str();
			LOG( ERROR, LOG_CATEGORY, "CTechnology: invalid tag %s for XML file", tagName );
			return false;
		}
	}
	return true;
}
bool CTechnology::isTechValid()
{
	if ( m_excluded || m_inProgress )
		return false;

	for( size_t i=0; i<m_Pairs.size(); i++ )
	{
		if( g_TechnologyCollection.getTechnology( m_Pairs[i], m_player )->m_inProgress )
			return false;
	}

	return ( hasReqEntities() && hasReqTechs() );
}

bool CTechnology::hasReqEntities()
{
	// Check whether we have ALL the required entities.

	std::vector<HEntity>* entities = m_player->GetControlledEntities();
	for ( std::vector<CStr>::iterator it=m_ReqEntities.begin(); it != m_ReqEntities.end(); it++ )
	{
		// For each required class, check that we have it
		bool got = false;
		for( CEntityList::iterator it2=entities->begin(); it2 != entities->end(); it2++ )
		{
			if ( (*it2)->m_classes.IsMember(*it) )
			{	
				got = true;
				break;
			}
		}
		if( !got )
		{
			delete entities;
			return false;
		}
	}
	delete entities;
	return true;
}

bool CTechnology::hasReqTechs()
{
	// Check whether we have ANY of the required techs (this is slightly confusing but required for 
	// the way the tech system is currently planned; ideally we'd have an <Or> or <And> in the XML).

	if ( m_ReqTechs.size() == 0 )
		return true;

	for ( std::vector<CStr>::iterator it=m_ReqTechs.begin(); it != m_ReqTechs.end(); it++ )
	{
		if ( g_TechnologyCollection.getTechnology( (CStrW)*it, m_player )->isResearched() )
		{	
			return true;
		}
	}
	return false;
}

void CTechnology::apply( CEntity* entity )
{
	// Find out if the unit has one of our target classes
	bool ok = false;
	for ( std::vector<CStr>::iterator it = m_Targets.begin(); it != m_Targets.end(); it++ )
	{
		if ( entity->m_classes.IsMember( *it ) )
		{
			ok = true;
			break;
		}
	}
	if( !ok ) return;

	// Apply modifiers
	for ( std::vector<Modifier>::iterator mod=m_Modifiers.begin(); mod!=m_Modifiers.end(); mod++ )
	{
		jsval oldVal;
		if( entity->GetProperty( g_ScriptingHost.getContext(), mod->attribute, &oldVal ) )
		{
			float modValue;
			if( mod->isPercent )
			{
				jsval baseVal;
				entity->m_base->m_unmodified->GetProperty( g_ScriptingHost.getContext(), mod->attribute, &baseVal );
				modValue = ToPrimitive<float>(baseVal) * mod->value / 100.0f;
			}
			else
			{
				modValue = mod->value;
			}

			jsval newVal = ToJSVal( ToPrimitive<float>(oldVal) + modValue );
			entity->SetProperty( g_ScriptingHost.GetContext(), mod->attribute, &newVal );
		}
	}

	// Apply sets
	for ( std::vector<Modifier>::iterator mod=m_Sets.begin(); mod!=m_Sets.end(); mod++ )
	{
		jsval newVal = ToJSVal( mod->value );
		entity->SetProperty( g_ScriptingHost.GetContext(), mod->attribute, &newVal );
	}
}


//JS stuff

void CTechnology::ScriptingInit()
{
	AddProperty(L"name", &CTechnology::m_Name, true);
	AddProperty(L"player", &CTechnology::m_player, true);
	AddProperty(L"generic", &CTechnology::m_Generic, true);
	AddProperty(L"specific", &CTechnology::m_Specific, true);
	AddProperty(L"icon", &CTechnology::m_Icon);	//GUI might want to change this...?
	AddProperty<int>(L"icon_cell", &CTechnology::m_IconCell);
	AddProperty(L"classes", &CTechnology::m_Classes, true);
	AddProperty(L"history", &CTechnology::m_History, true);

	AddProperty<float>(L"time", &CTechnology::m_ReqTime);	//Techs may upgrade research time and cost of other techs
	AddProperty<float>(L"food", &CTechnology::m_ReqFood);
	AddProperty<float>(L"wood", &CTechnology::m_ReqWood);
	AddProperty<float>(L"stone", &CTechnology::m_ReqStone);
	AddProperty<float>(L"metal", &CTechnology::m_ReqMetal);
	AddProperty<bool>(L"in_progress", &CTechnology::m_inProgress);

	AddMethod<jsval, &CTechnology::ApplyEffects>( "applyEffects", 2 );
	AddMethod<jsval, &CTechnology::IsExcluded>( "isExcluded", 0 );
	AddMethod<jsval, &CTechnology::IsValid>( "isValid", 0 );
	AddMethod<jsval, &CTechnology::IsResearched>( "isResearched", 0 );
	AddMethod<jsval, &CTechnology::GetPlayerID>( "getPlayerID", 0 );

	CJSObject<CTechnology>::ScriptingInit("Technology");
}

jsval CTechnology::ApplyEffects( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv) )
{
	// Unmark ourselves as in progress
	m_inProgress = false;

	if ( !isTechValid() )
	{
		return JSVAL_FALSE;
	}

	// Disable any paired techs
	for ( std::vector<CStr>::iterator it=m_Pairs.begin(); it != m_Pairs.end(); it++ )
		g_TechnologyCollection.getTechnology(*it, m_player)->setExclusion(true);

	// Disable ourselves so we can't be researched twice
	m_excluded = true;

	// Mark ourselves as researched
	m_researched = true;

	// Apply effects to all entities
	std::vector<HEntity>* entities = m_player->GetControlledEntities();
	for ( size_t i=0; i<entities->size(); ++i )
	{
		apply( (*entities)[i] );
	}
	delete entities;
	
	// Run one-time tech script
	if( m_effectFunction )
	{
		jsval rval;
		jsval arg = ToJSVal( m_player );
		m_effectFunction.Run( this->GetScript(), &rval, 1, &arg );
	}

	// Add ourselves to player's researched techs
	m_player->AddActiveTech( this );

	return JSVAL_TRUE;
}

jsval CTechnology::IsValid( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv) )
{
	return ToJSVal( isTechValid() );
}

jsval CTechnology::IsExcluded( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv) )
{
	return ToJSVal( m_excluded );
}

jsval CTechnology::IsResearched( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv) )
{
	return ToJSVal( isResearched() );
}

inline jsval CTechnology::GetPlayerID( JSContext* UNUSED(cx), uintN UNUSED(argc), jsval* UNUSED(argv) )
{
	return ToJSVal( m_player->GetPlayerID() );
}




